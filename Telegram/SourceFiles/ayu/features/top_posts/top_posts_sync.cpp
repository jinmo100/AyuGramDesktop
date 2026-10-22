// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/top_posts/top_posts_sync.h"

#include "ayu/data/ayu_database.h"
#include "ayu/features/top_posts/top_posts_scanner.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "crl/crl_async.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace TopPosts {

IncrementalSyncController::IncrementalSyncController(
	not_null<PeerData*> peer)
: _peer(peer)
, _channelId(getBareID(peer))
, _api(&peer->session().mtp()) {
	_throttleTimer.setCallback([=] {
		if (_progress.state == SyncState::SyncingDeltas) {
			requestNextDeltaBatch();
		} else if (_progress.state == SyncState::RefreshingCached) {
			requestNextCachedChunk();
		}
	});
}

IncrementalSyncController::~IncrementalSyncController() {
	_throttleTimer.cancel();
	_countdownTimer.cancel();
	if (_requestId) {
		_api.request(base::take(_requestId)).cancel();
	}
}

void IncrementalSyncController::start() {
	if (_progress.state == SyncState::SyncingDeltas
		|| _progress.state == SyncState::RefreshingCached) {
		return;
	}

	_highestCachedMessageId = AyuDatabase::getChannelTopPostsMaxMessageId(_channelId);
	const auto cachedIds = AyuDatabase::getChannelTopPostMessageIds(_channelId);
	_cachedPostsMap.clear();
	const auto cachedPosts = AyuDatabase::getChannelTopPosts(
		_channelId,
		AyuDatabase::TopPostsSortOrder::Reactions,
		AyuDatabase::TopPostsFilter::All,
		300);
	for (const auto &post : cachedPosts) {
		_cachedPostsMap.emplace(post.messageId, post);
	}

	_cachedChunks.clear();
	for (size_t i = 0; i < cachedIds.size(); i += kBatchSize) {
		const auto end = std::min(i + kBatchSize, cachedIds.size());
		_cachedChunks.emplace_back(cachedIds.begin() + i, cachedIds.begin() + end);
	}
	_currentChunkIndex = 0;

	_deltaPosts.clear();
	_refreshedPosts.clear();
	_allUpdatedPosts.clear();
	_deltaAggregator.reset();
	_deltaOffsetId = 0;

	_progress.deltasFetched = 0;
	_progress.refreshedCount = 0;
	_progress.totalToRefresh = static_cast<int>(cachedIds.size());
	_progress.floodWaitSeconds = 0;
	_progress.errorMessage.clear();

	if (_highestCachedMessageId > 0) {
		startDeltaPhase();
	} else {
		startRefreshPhase();
	}
}

void IncrementalSyncController::cancel() {
	if (_progress.state == SyncState::Completed
		|| _progress.state == SyncState::Cancelled
		|| _progress.state == SyncState::Failed) {
		return;
	}
	finish(SyncState::Cancelled);
}

not_null<PeerData*> IncrementalSyncController::peer() const {
	return _peer;
}

ID IncrementalSyncController::channelId() const {
	return _channelId;
}

SyncProgress IncrementalSyncController::progressValue() const {
	return _progress;
}

rpl::producer<SyncProgress> IncrementalSyncController::progress() const {
	return _progressStream.events();
}

rpl::producer<std::vector<ChannelTopPost>> IncrementalSyncController::updatedPosts() const {
	return _updatedPostsStream.events();
}

const std::vector<ChannelTopPost> &IncrementalSyncController::allUpdatedPosts() const {
	return _allUpdatedPosts;
}

void IncrementalSyncController::startDeltaPhase() {
	_progress.state = SyncState::SyncingDeltas;
	_progressStream.fire_copy(_progress);
	requestNextDeltaBatch();
}

void IncrementalSyncController::requestNextDeltaBatch() {
	if (_progress.state != SyncState::SyncingDeltas) {
		return;
	}

	_requestId = _api.request(MTPmessages_GetHistory(
		_peer->input(),
		MTP_int(_deltaOffsetId.bare),
		MTP_int(0),
		MTP_int(0),
		MTP_int(kBatchSize),
		MTP_int(0),
		MTP_int(_highestCachedMessageId),
		MTP_long(0)
	)).done([=](const MTPmessages_Messages &result) {
		_requestId = 0;
		handleDeltaResponse(result);
	}).fail([=](const MTP::Error &error) {
		_requestId = 0;
		handleError(error);
	}).send();
}

void IncrementalSyncController::handleDeltaResponse(
		const MTPmessages_Messages &result) {
	if (_progress.state != SyncState::SyncingDeltas) {
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
		const auto flushed = _deltaAggregator.flush();
		_deltaPosts.insert(_deltaPosts.end(), flushed.begin(), flushed.end());
		startRefreshPhase();
		return;
	}

	auto batchPosts = std::vector<ChannelTopPost>();
	batchPosts.reserve(messages->size());

	const auto scanTime = base::unixtime::now();
	auto oldestId = MsgId(0);

	for (const auto &msg : *messages) {
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
		++_progress.deltasFetched;
	}

	if (!batchPosts.empty()) {
		const auto coalesced = _deltaAggregator.feed(batchPosts);
		_deltaPosts.insert(_deltaPosts.end(), coalesced.begin(), coalesced.end());
	}

	_progressStream.fire_copy(_progress);

	if (!oldestId
		|| oldestId == _deltaOffsetId
		|| oldestId.bare <= _highestCachedMessageId
		|| messages->size() < kBatchSize) {
		const auto flushed = _deltaAggregator.flush();
		_deltaPosts.insert(_deltaPosts.end(), flushed.begin(), flushed.end());
		startRefreshPhase();
		return;
	}

	_deltaOffsetId = oldestId;
	const auto delay = CalculateThrottleDelay();
	_throttleTimer.callOnce(delay);
}

void IncrementalSyncController::startRefreshPhase() {
	if (_currentChunkIndex >= _cachedChunks.size()) {
		finish(SyncState::Completed);
		return;
	}
	_progress.state = SyncState::RefreshingCached;
	_progressStream.fire_copy(_progress);
	requestNextCachedChunk();
}

void IncrementalSyncController::requestNextCachedChunk() {
	if (_progress.state != SyncState::RefreshingCached) {
		return;
	}
	if (_currentChunkIndex >= _cachedChunks.size()) {
		finish(SyncState::Completed);
		return;
	}

	const auto &chunk = _cachedChunks[_currentChunkIndex];
	auto mtpIds = QVector<MTPInputMessage>();
	mtpIds.reserve(chunk.size());
	for (const auto id : chunk) {
		mtpIds.push_back(MTP_inputMessageID(MTP_int(id)));
	}

	if (const auto channel = _peer->asChannel()) {
		_requestId = _api.request(MTPchannels_GetMessages(
			channel->inputChannel(),
			MTP_vector<MTPInputMessage>(mtpIds)
		)).done([=](const MTPmessages_Messages &result) {
			_requestId = 0;
			handleRefreshResponse(result);
		}).fail([=](const MTP::Error &error) {
			_requestId = 0;
			handleError(error);
		}).send();
	} else {
		_requestId = _api.request(MTPmessages_GetMessages(
			MTP_vector<MTPInputMessage>(mtpIds)
		)).done([=](const MTPmessages_Messages &result) {
			_requestId = 0;
			handleRefreshResponse(result);
		}).fail([=](const MTP::Error &error) {
			_requestId = 0;
			handleError(error);
		}).send();
	}
}

void IncrementalSyncController::handleRefreshResponse(
		const MTPmessages_Messages &result) {
	if (_progress.state != SyncState::RefreshingCached) {
		return;
	}

	const QVector<MTPMessage> *messages = nullptr;
	result.match([&](const MTPDmessages_messagesNotModified &) {
	}, [&](const auto &data) {
		_peer->owner().processUsers(data.vusers());
		_peer->owner().processChats(data.vchats());
		messages = &data.vmessages().v;
	});

	if (messages) {
		const auto scanTime = base::unixtime::now();
		for (const auto &msg : *messages) {
			if (msg.type() == mtpc_messageEmpty) {
				const auto emptyId = msg.c_messageEmpty().vid().v;
				AyuDatabase::removePost(_channelId, emptyId);
				++_progress.refreshedCount;
				continue;
			}
			const auto item = _peer->owner().addNewMessage(
				msg,
				MessageFlags(),
				NewMessageType::Existing);
			if (item && !item->isService()) {
				auto updated = ExtractPostMetrics(item, _channelId, scanTime);
				if (const auto it = _cachedPostsMap.find(updated.messageId); it != _cachedPostsMap.end()) {
					const auto &cached = it->second;
					if (cached.isGrouped()) {
						updated.setType(TopPostMediaType::Grouped);
						updated.groupedId = cached.groupedId;
						updated.groupedCount = std::max(1, cached.groupedCount);
						updated.reactionsCount = std::max(cached.reactionsCount, updated.reactionsCount);
						updated.commentsCount = std::max(cached.commentsCount, updated.commentsCount);
						updated.viewsCount = std::max(cached.viewsCount, updated.viewsCount);
						updated.reactionsJson = MergeReactionsJson(cached.reactionsJson, updated.reactionsJson);
					}
				}
				_refreshedPosts.push_back(std::move(updated));
			}
			++_progress.refreshedCount;
		}
	}

	++_currentChunkIndex;
	_progressStream.fire_copy(_progress);

	if (_currentChunkIndex >= _cachedChunks.size()) {
		finish(SyncState::Completed);
		return;
	}

	const auto delay = CalculateThrottleDelay();
	_throttleTimer.callOnce(delay);
}

void IncrementalSyncController::handleError(const MTP::Error &error) {
	if (_progress.state != SyncState::SyncingDeltas
		&& _progress.state != SyncState::RefreshingCached) {
		return;
	}
	const auto type = error.type();
	const auto floodSeconds = ParseFloodWaitSeconds(type);
	if (floodSeconds > 0) {
		handleFloodWait(floodSeconds);
		return;
	}
	finish(SyncState::Failed, type);
}

void IncrementalSyncController::handleFloodWait(int seconds) {
	_throttleTimer.cancel();
	const auto previousState = _progress.state;
	_progress.state = SyncState::PausedFloodWait;
	_progress.floodWaitSeconds = std::max(seconds, 1);
	_progressStream.fire_copy(_progress);

	_countdownTimer.setCallback([=] {
		--_progress.floodWaitSeconds;
		if (_progress.floodWaitSeconds <= 0) {
			_countdownTimer.cancel();
			_progress.state = previousState;
			_progressStream.fire_copy(_progress);
			if (previousState == SyncState::SyncingDeltas) {
				requestNextDeltaBatch();
			} else {
				requestNextCachedChunk();
			}
		} else {
			_progressStream.fire_copy(_progress);
		}
	});
	_countdownTimer.callEach(1000);
}

void IncrementalSyncController::finish(
		SyncState finalState,
		const QString &error) {
	_throttleTimer.cancel();
	_countdownTimer.cancel();
	if (_requestId) {
		_api.request(base::take(_requestId)).cancel();
	}

	if (finalState == SyncState::Completed) {
		_allUpdatedPosts.clear();
		_allUpdatedPosts.reserve(_deltaPosts.size() + _refreshedPosts.size());
		_allUpdatedPosts.insert(
			_allUpdatedPosts.end(),
			_deltaPosts.begin(),
			_deltaPosts.end());
		_allUpdatedPosts.insert(
			_allUpdatedPosts.end(),
			_refreshedPosts.begin(),
			_refreshedPosts.end());

		if (!_allUpdatedPosts.empty()) {
			_updatedPostsStream.fire_copy(_allUpdatedPosts);
			crl::async([this, posts = _allUpdatedPosts, channelId = _channelId, weak = base::make_weak(this), finalState, error] {
				AyuDatabase::saveChannelPosts(posts);
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
	}

	_progress.state = finalState;
	_progress.errorMessage = error;
	_progress.floodWaitSeconds = 0;
	_progressStream.fire_copy(_progress);
}

} // namespace TopPosts
