# 03: Media Group Aggregator & Incremental Sync Engine

Blocked by: 01, 02

## Summary
Implement the Media Group aggregation logic that merges contiguous items sharing a `grouped_id` into unified entries, along with the dual-phase Incremental Sync engine that updates existing cached Top Posts.

## Acceptance Criteria
- [x] Contiguous messages sharing identical `grouped_id` are coalesced into a single `ChannelTopPost` representing the album/set.
- [x] Aggregated card preserves the anchor `messageId` (the first/lead message in the set) for proper timeline jumping.
- [x] Total reaction counts across all messages in the media group are aggregated into the top-level metric.
- [x] Incremental Sync fetches message deltas since the highest cached `messageId` to capture newly published posts.
- [x] Incremental Sync executes a batched `messages.getMessages` query for the existing cached Top 300 IDs to refresh their reaction and comment counts without full re-scanning.
- [x] Post-sync database update triggers re-ranking and re-truncation to Top 300.

## Implementation Details
- Add aggregation pipeline in `Telegram/SourceFiles/ayu/features/top_posts/top_posts_aggregator.h` and `.cpp`.
- Add `IncrementalSyncController` in `Telegram/SourceFiles/ayu/features/top_posts/top_posts_sync.h` and `.cpp`.
- Connects scanner output to `AyuDatabase` transaction batches.
