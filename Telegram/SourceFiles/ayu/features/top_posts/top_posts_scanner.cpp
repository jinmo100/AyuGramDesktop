// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/top_posts/top_posts_scanner.h"

#include "ayu/data/ayu_database.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace TopPosts {

int ParseFloodWaitSeconds(const QString &errorType) {
	if (errorType.startsWith(u"FLOOD_WAIT_"_q)
		|| errorType.startsWith(u"FLOOD_PREMIUM_WAIT_"_q)) {
		const auto underscore = errorType.lastIndexOf('_');
		if (underscore >= 0) {
			const auto sec = errorType.mid(underscore + 1).toInt();
			return std::max(sec, 1);
		}
		return 1;
	}
	return 0;
}

crl::time CalculateThrottleDelay(int minMs, int maxMs) {
	if (maxMs <= minMs) {
		return crl::time(minMs);
	}
	const auto diff = maxMs - minMs + 1;
	const auto r = std::abs(base::RandomValue<int>()) % diff;
	return crl::time(minMs + r);
}

ChannelTopPost ExtractPostMetrics(
		not_null<HistoryItem*> item,
		ID channelId,
		int scanTime) {
	auto post = ChannelTopPost();
	post.channelId = channelId;
	post.messageId = item->id.bare;
	post.date = item->date();
	post.scanTime = scanTime;

	auto reactionsCount = 0;
	auto reactionsObj = QJsonObject();
	for (const auto &r : item->reactions()) {
		reactionsCount += r.count;
		auto key = QString();
		if (r.id.paid()) {
			key = u"⭐"_q;
		} else if (!r.id.emoji().isEmpty()) {
			key = r.id.emoji();
		} else if (r.id.custom()) {
			key = QString::number(r.id.custom());
		}
		if (!key.isEmpty()) {
			reactionsObj[key] = r.count;
		}
	}
	post.reactionsCount = reactionsCount;
	post.reactionsJson = QString::fromUtf8(
		QJsonDocument(reactionsObj).toJson(QJsonDocument::Compact)
	).toStdString();

	post.commentsCount = item->repliesAreComments() ? item->repliesCount() : 0;
	post.viewsCount = item->viewsCount();

	const auto groupId = item->groupId();
	post.groupedId = groupId ? static_cast<ID>(groupId.raw()) : ID(0);

	if (groupId) {
		post.setType(TopPostMediaType::Grouped);
	} else if (const auto media = item->media()) {
		if (media->photo()) {
			post.setType(TopPostMediaType::Photo);
		} else if (const auto doc = media->document()) {
			if (doc->isVideoFile() || doc->isVideoMessage()) {
				post.setType(TopPostMediaType::Video);
			} else {
				post.setType(TopPostMediaType::Document);
			}
		} else {
			post.setType(TopPostMediaType::None);
		}
	} else {
		post.setType(TopPostMediaType::None);
	}

	auto snippet = item->originalText().text.trimmed();
	if (snippet.isEmpty()) {
		if (const auto media = item->media()) {
			if (const auto doc = media->document()) {
				snippet = doc->filename().trimmed();
			}
		}
	}
	if (snippet.size() > 120) {
		snippet = snippet.left(120);
	}
	post.textSnippet = snippet.toStdString();

	return post;
}

Scanner::Scanner(
	not_null<PeerData*> peer,
	int scanWindow,
	TopPostsFilter filter)
: _peer(peer)
, _channelId(peer->id.bare)
, _scanWindow(std::clamp(scanWindow, 1, kMaxScanWindow))
, _filter(filter)
, _api(&peer->session().mtp()) {
	_throttleTimer.setCallback([=] {
		requestNextBatch();
	});
}

Scanner::~Scanner() {
	_throttleTimer.cancel();
	_countdownTimer.cancel();
	if (_requestId) {
		_api.request(base::take(_requestId)).cancel();
	}
}

void Scanner::start() {
	if (_progress.state == ScanState::Scanning) {
		return;
	}
	_scannedCount = 0;
	_matchedCount = 0;
	_offsetId = 0;
	_allParsedPosts.clear();
	_aggregator.reset();
	_progress.scannedCount = 0;
	_progress.matchedCount = 0;
	_progress.targetCount = _scanWindow;
	_progress.state = ScanState::Scanning;
	_progress.errorMessage.clear();
	_progress.floodWaitSeconds = 0;
	_progressStream.fire_copy(_progress);

	requestNextBatch();
}

void Scanner::pause() {
	if (_progress.state != ScanState::Scanning) {
		return;
	}
	_throttleTimer.cancel();
	_progress.state = ScanState::Idle;
	_progressStream.fire_copy(_progress);
}

void Scanner::resume() {
	if (_progress.state != ScanState::Idle) {
		return;
	}
	_progress.state = ScanState::Scanning;
	_progressStream.fire_copy(_progress);
	requestNextBatch();
}

void Scanner::cancel() {
	if (_progress.state == ScanState::Completed
		|| _progress.state == ScanState::Cancelled
		|| _progress.state == ScanState::Failed) {
		return;
	}
	finish(ScanState::Cancelled);
}

not_null<PeerData*> Scanner::peer() const {
	return _peer;
}

ID Scanner::channelId() const {
	return _channelId;
}

int Scanner::scanWindow() const {
	return _scanWindow;
}

TopPostsFilter Scanner::filter() const {
	return _filter;
}

ScanProgress Scanner::progressValue() const {
	return _progress;
}

rpl::producer<ScanProgress> Scanner::progress() const {
	return _progressStream.events();
}

rpl::producer<std::vector<ChannelTopPost>> Scanner::batchParsed() const {
	return _batchParsedStream.events();
}

const std::vector<ChannelTopPost> &Scanner::allParsedPosts() const {
	return _allParsedPosts;
}

void Scanner::setAutoSave(bool enable) {
	_autoSave = enable;
}

bool Scanner::autoSave() const {
	return _autoSave;
}

void Scanner::requestNextBatch() {
	if (_progress.state != ScanState::Scanning) {
		return;
	}
	if (_scannedCount >= _scanWindow) {
		finish(ScanState::Completed);
		return;
	}

	const auto remaining = _scanWindow - _scannedCount;
	const auto batchLimit = std::clamp(remaining, 1, kBatchSize);

	if (_filter == TopPostsFilter::All) {
		_requestId = _api.request(MTPmessages_GetHistory(
			_peer->input(),
			MTP_int(_offsetId.bare),
			MTP_int(0),
			MTP_int(0),
			MTP_int(batchLimit),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)
		)).done([=](const MTPmessages_Messages &result) {
			_requestId = 0;
			handleResponse(result);
		}).fail([=](const MTP::Error &error) {
			_requestId = 0;
			handleError(error);
		}).send();
	} else {
		const auto mtpFilter = (_filter == TopPostsFilter::Documents)
			? MTP_inputMessagesFilterDocument()
			: MTP_inputMessagesFilterPhotoVideo();
		_requestId = _api.request(MTPmessages_Search(
			MTP_flags(0),
			_peer->input(),
			MTP_string(),
			MTP_inputPeerEmpty(),
			MTPInputPeer(),
			MTPVector<MTPReaction>(),
			MTP_int(0),
			mtpFilter,
			MTP_int(0),
			MTP_int(0),
			MTP_int(_offsetId.bare),
			MTP_int(0),
			MTP_int(batchLimit),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)
		)).done([=](const MTPmessages_Messages &result) {
			_requestId = 0;
			handleResponse(result);
		}).fail([=](const MTP::Error &error) {
			_requestId = 0;
			handleError(error);
		}).send();
	}
}

void Scanner::handleResponse(const MTPmessages_Messages &result) {
	if (_progress.state != ScanState::Scanning) {
		return;
	}

	const QVector<MTPMessage> *messages = nullptr;
	result.match([&](const MTPDmessages_messagesNotModified &) {
	}, [&](const auto &data) {
		_peer->owner().processUsers(data.vusers());
		_peer->owner().processChats(data.vchats());
		messages = &data.vmessages().v;
	});

	if (!messages || messages->isEmpty()) {
		finish(ScanState::Completed);
		return;
	}

	auto batchPosts = std::vector<ChannelTopPost>();
	batchPosts.reserve(messages->size());

	const auto scanTime = base::unixtime::now();
	auto oldestId = MsgId(0);

	for (const auto &msg : *messages) {
		++_scannedCount;
		const auto item = _peer->owner().addNewMessage(
			msg,
			MessageFlags(),
			NewMessageType::Existing);
		if (!item) {
			continue;
		}
		const auto msgId = item->id;
		if (!oldestId || msgId < oldestId) {
			oldestId = msgId;
		}
		if (item->isService()) {
			continue;
		}

		batchPosts.push_back(ExtractPostMetrics(item, _channelId, scanTime));
		++_matchedCount;
	}

	_progress.scannedCount = _scannedCount;
	_progress.matchedCount = _matchedCount;
	_progress.targetCount = _scanWindow;

	if (!batchPosts.empty()) {
		const auto coalesced = _aggregator.feed(batchPosts);
		if (!coalesced.empty()) {
			_allParsedPosts.insert(
				_allParsedPosts.end(),
				coalesced.begin(),
				coalesced.end());

			if (_autoSave) {
				crl::async([posts = coalesced] {
					AyuDatabase::saveChannelPosts(posts);
				});
			}

			_batchParsedStream.fire_copy(coalesced);
		}
	}

	_progressStream.fire_copy(_progress);

	if (!oldestId
		|| oldestId == _offsetId
		|| _scannedCount >= _scanWindow
		|| messages->size() < kBatchSize) {
		finish(ScanState::Completed);
		return;
	}

	_offsetId = oldestId;

	const auto delay = CalculateThrottleDelay();
	_throttleTimer.callOnce(delay);
}

void Scanner::handleError(const MTP::Error &error) {
	if (_progress.state != ScanState::Scanning) {
		return;
	}
	const auto type = error.type();
	const auto floodSeconds = ParseFloodWaitSeconds(type);
	if (floodSeconds > 0) {
		handleFloodWait(floodSeconds);
		return;
	}
	finish(ScanState::Failed, type);
}

void Scanner::handleFloodWait(int seconds) {
	_throttleTimer.cancel();
	_progress.state = ScanState::PausedFloodWait;
	_progress.floodWaitSeconds = std::max(seconds, 1);
	_progressStream.fire_copy(_progress);

	_countdownTimer.setCallback([=] {
		--_progress.floodWaitSeconds;
		if (_progress.floodWaitSeconds <= 0) {
			_countdownTimer.cancel();
			_progress.state = ScanState::Scanning;
			_progressStream.fire_copy(_progress);
			requestNextBatch();
		} else {
			_progressStream.fire_copy(_progress);
		}
	});
	_countdownTimer.callEach(1000);
}

void Scanner::finish(ScanState finalState, const QString &error) {
	_throttleTimer.cancel();
	_countdownTimer.cancel();
	if (_requestId) {
		_api.request(base::take(_requestId)).cancel();
	}

	if (finalState == ScanState::Completed) {
		const auto flushed = _aggregator.flush();
		if (!flushed.empty()) {
			_allParsedPosts.insert(
				_allParsedPosts.end(),
				flushed.begin(),
				flushed.end());
			_batchParsedStream.fire_copy(flushed);
		}
		if (_autoSave) {
			crl::async([posts = flushed, channelId = _channelId, weak = base::make_weak(this), finalState, error] {
				if (!posts.empty()) {
					AyuDatabase::saveChannelPosts(posts);
				}
				AyuDatabase::truncateChannelPosts(channelId, 300);
				crl::on_main(weak, [=] {
					_progress.state = finalState;
					_progress.errorMessage = error;
					_progress.floodWaitSeconds = 0;
					_progressStream.fire_copy(_progress);
				});
			});
			return;
		}
	} else if (finalState == ScanState::Cancelled && _autoSave) {
		crl::async([channelId = _channelId, weak = base::make_weak(this), finalState, error] {
			AyuDatabase::truncateChannelPosts(channelId, 300);
			crl::on_main(weak, [=] {
				_progress.state = finalState;
				_progress.errorMessage = error;
				_progress.floodWaitSeconds = 0;
				_progressStream.fire_copy(_progress);
			});
		});
		return;
	}

	_progress.state = finalState;
	_progress.errorMessage = error;
	_progress.floodWaitSeconds = 0;
	_progressStream.fire_copy(_progress);
}

} // namespace TopPosts
