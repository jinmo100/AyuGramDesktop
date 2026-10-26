// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/ayu_database.h"
#include "ayu/data/entities.h"
#include "ayu/features/top_posts/top_posts_aggregator.h"
#include "base/basic_types.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "mtproto/sender.h"
#include "rpl/event_stream.h"
#include "rpl/producer.h"

class PeerData;
class HistoryItem;

namespace TopPosts {

inline constexpr auto kDefaultScanWindow = 1000;
inline constexpr auto kMinScanWindow = 100;
inline constexpr auto kMaxScanWindow = 5000;
inline constexpr auto kBatchSize = 100;
inline constexpr auto kMaxRangeScanMessages = 20000;
inline constexpr auto kMaxRangeScanTotal = 200000;
inline constexpr auto kThrottleMinMs = 150;
inline constexpr auto kThrottleMaxMs = 250;

enum class ScanState {
	Idle,
	Scanning,
	PausedFloodWait,
	Completed,
	Cancelled,
	Failed,
};

struct ScanProgress {
	ScanState state = ScanState::Idle;
	int scannedCount = 0;
	int targetCount = 0;
	int matchedCount = 0;
	int floodWaitSeconds = 0;
	bool dateRange = false;
	QString errorMessage;
};

[[nodiscard]] int ParseFloodWaitSeconds(const QString &errorType);
[[nodiscard]] crl::time CalculateThrottleDelay(
	int minMs = kThrottleMinMs,
	int maxMs = kThrottleMaxMs);
[[nodiscard]] ChannelTopPost ExtractPostMetrics(
	not_null<HistoryItem*> item,
	ID channelId,
	int scanTime);

class Scanner final : public base::has_weak_ptr {
public:
	Scanner(
		not_null<PeerData*> peer,
		int scanWindow = kDefaultScanWindow,
		MsgId resumeOffset = 0,
		int alreadyScanned = 0,
		TimeId rangeFrom = 0,
		TimeId rangeTo = 0);
	~Scanner();

	void start();
	void pause();
	void resume();
	void cancel();

	[[nodiscard]] not_null<PeerData*> peer() const;
	[[nodiscard]] ID channelId() const;
	[[nodiscard]] int scanWindow() const;
	[[nodiscard]] MsgId resumeOffset() const;
	[[nodiscard]] TimeId rangeFrom() const;
	[[nodiscard]] TimeId rangeTo() const;

	[[nodiscard]] ScanProgress progressValue() const;
	[[nodiscard]] rpl::producer<ScanProgress> progress() const;
	[[nodiscard]] rpl::producer<std::vector<ChannelTopPost>> batchParsed() const;
	[[nodiscard]] const std::vector<ChannelTopPost> &allParsedPosts() const;

	void setAutoSave(bool enable);
	[[nodiscard]] bool autoSave() const;

private:
	void requestNextBatch();
	void handleResponse(const MTPmessages_Messages &result);
	void handleError(const MTP::Error &error);
	void handleFloodWait(int seconds);
	void finish(ScanState finalState, const QString &error = QString());
	void writeResultsAsync(
		std::vector<ChannelTopPost> posts,
		ScanState finalState,
		const QString &error);

	const not_null<PeerData*> _peer;
	const ID _channelId = 0;
	const int _scanWindow = kDefaultScanWindow;
	const MsgId _resumeOffset = 0;
	const int _alreadyScanned = 0;
	const TimeId _rangeFrom = 0;
	const TimeId _rangeTo = 0;

	MTP::Sender _api;
	mtpRequestId _requestId = 0;

	MsgId _offsetId = 0;
	MsgId _rangeFirstId = 0;
	int _scannedCount = 0;
	int _matchedCount = 0;

	base::Timer _throttleTimer;
	base::Timer _countdownTimer;

	bool _autoSave = true;
	ScanProgress _progress;
	rpl::event_stream<ScanProgress> _progressStream;
	rpl::event_stream<std::vector<ChannelTopPost>> _batchParsedStream;
	MediaGroupAggregator _aggregator;
	std::vector<ChannelTopPost> _allParsedPosts;

};

} // namespace TopPosts
