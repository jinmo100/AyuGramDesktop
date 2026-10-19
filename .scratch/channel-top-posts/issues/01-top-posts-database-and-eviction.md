# 01: Local Persistence Schema & Eviction Engine (`ChannelTopPost`)

Blocked by: None

## Summary
Add the `ChannelTopPost` entity schema to `AyuDatabase` (`./tdata/ayudata.db`) along with query methods, per-channel Top 300 quota truncation, 30-day TTL expiration, and 30-channel global LRU eviction.

## Acceptance Criteria
- [x] Database table `ChannelTopPost` successfully created with fields matching the spec (channelId, messageId, date, reactionsCount, commentsCount, viewsCount, textSnippet, mediaType, reactionsJson, scanTime, groupedId).
- [x] Batch insert / upsert method correctly updates existing message records without creating duplicates.
- [x] Post-scan truncation method trims the records for a given channel to strictly the top 300 sorted by Reaction Engagement and comments.
- [x] Prune routine deletes records with `scanTime` older than 30 days.
- [x] Global LRU routine ensures no more than 30 distinct channels retain cached data, pruning the oldest scanned channels first.
- [x] `clearChannelPosts(channelId)` completely removes all rows for a target channel.
- [x] `removePost(channelId, messageId)` deletes a specific record when a message is revoked.

## Implementation Details
- Add struct `ChannelTopPost` in `Telegram/SourceFiles/ayu/data/entities.h`.
- Register the table and composite indices in `Telegram/SourceFiles/ayu/data/ayu_database.cpp` using `sqlite_orm`.
- Expose query and eviction methods in `Telegram/SourceFiles/ayu/data/ayu_database.h`.
