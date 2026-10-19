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
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "mtproto/sender.h"
#include "rpl/event_stream.h"
#include "rpl/producer.h"

class PeerData;

namespace TopPosts {

enum class SyncState {
	Idle,
	SyncingDeltas,
	RefreshingCached,
	PausedFloodWait,
	Completed,
	Cancelled,
	Failed,
};

struct SyncProgress {
	SyncState state = SyncState::Idle;
	int deltasFetched = 0;
	int refreshedCount = 0;
	int totalToRefresh = 0;
	int floodWaitSeconds = 0;
	QString errorMessage;
};

class IncrementalSyncController final : public base::has_weak_ptr {
public:
	IncrementalSyncController(
		not_null<PeerData*> peer,
		TopPostsFilter filter = TopPostsFilter::All);
	~IncrementalSyncController();

	void start();
	void cancel();

	[[nodiscard]] not_null<PeerData*> peer() const;
	[[nodiscard]] ID channelId() const;
	[[nodiscard]] TopPostsFilter filter() const;

	[[nodiscard]] SyncProgress progressValue() const;
	[[nodiscard]] rpl::producer<SyncProgress> progress() const;
	[[nodiscard]] rpl::producer<std::vector<ChannelTopPost>> updatedPosts() const;
	[[nodiscard]] const std::vector<ChannelTopPost> &allUpdatedPosts() const;

private:
	void startDeltaPhase();
	void requestNextDeltaBatch();
	void handleDeltaResponse(const MTPmessages_Messages &result);

	void startRefreshPhase();
	void requestNextCachedChunk();
	void handleRefreshResponse(const MTPmessages_Messages &result);

	void handleError(const MTP::Error &error);
	void handleFloodWait(int seconds);
	void finish(SyncState finalState, const QString &error = QString());

	const not_null<PeerData*> _peer;
	const ID _channelId = 0;
	const TopPostsFilter _filter = TopPostsFilter::All;

	MTP::Sender _api;
	mtpRequestId _requestId = 0;

	int _highestCachedMessageId = 0;
	MsgId _deltaOffsetId = 0;
	MediaGroupAggregator _deltaAggregator;
	std::vector<ChannelTopPost> _deltaPosts;

	std::vector<std::vector<int>> _cachedChunks;
	size_t _currentChunkIndex = 0;
	std::vector<ChannelTopPost> _refreshedPosts;

	base::Timer _throttleTimer;
	base::Timer _countdownTimer;

	SyncProgress _progress;
	rpl::event_stream<SyncProgress> _progressStream;
	rpl::event_stream<std::vector<ChannelTopPost>> _updatedPostsStream;
	std::vector<ChannelTopPost> _allUpdatedPosts;
};

} // namespace TopPosts
