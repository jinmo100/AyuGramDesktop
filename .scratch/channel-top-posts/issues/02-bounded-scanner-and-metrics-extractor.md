# 02: Bounded Asynchronous Message Scanner & Rate-Limiter

Blocked by: 01

## Summary
Implement the `TopPosts::Scanner` controller that fetches channel messages in bounded batches of 100, enforces a 150-250ms throttle delay between requests, captures Reaction Engagement, comments, and views from `HistoryItem`, and handles `FLOOD_WAIT` backoff and user cancellation.

## Acceptance Criteria
- [x] Scanner accepts a target channel `PeerData*`, a Scan Window size (default 1000, configurable up to 5000), and an optional `MessagesFilter` (All, Documents, Photos/Videos).
- [x] Requests messages using `MTPmessages_Search` / `MTPmessages_GetHistory` in sequential pages of 100 items.
- [x] Mandatory throttle delay of 150ms-250ms is observed between sequential network dispatches.
- [x] Correctly reads `MessageReactions` total counts, `repliesCount()` (and verifies `repliesAreComments()`), and `viewsCount()` from each item.
- [x] Emits reactive progress events (messages scanned, target count, items matched) for UI feedback.
- [x] Gracefully pauses and signals countdown on `FLOOD_WAIT` response instead of crashing or flooding.
- [x] Supports immediate cancellation via `cancel()` without leaking in-flight requests or leaving dangling callbacks.

## Implementation Details
- Create `Telegram/SourceFiles/ayu/features/top_posts/top_posts_scanner.h` and `.cpp`.
- Owns an `MTP::Sender` to tie request lifetime to the scanner instance.
- Uses `crl::async` for background calculation and dispatches parsed records to `crl::on_main` / database worker.
