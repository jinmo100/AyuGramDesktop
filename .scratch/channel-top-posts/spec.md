# Spec: Channel Top Posts Discovery & Ranking

## Problem Statement

Users navigating large resource-sharing broadcast channels on Telegram struggle to discover historical high-value, highly-acclaimed posts and files. Channel feeds grow linearly over time, often reaching tens of thousands of messages. Currently, clients only allow viewing messages in reverse chronological order (newest first) or searching by plain text keywords. To find the most popular or well-received resources, users must manually scroll through weeks or months of history. 

Furthermore, Telegram's MTProto API does not offer server-side sorting by reaction count or comment volume. Any naive client-side approach attempting to fetch all channel messages unbounded quickly triggers strict `FLOOD_WAIT` rate limits and runs the risk of account suspension. Users need a safe, bounded, locally-cached mechanism to discover and rank the most engaging posts in broadcast channels by reaction engagement, comment volume, and views.

## Solution

A dedicated "Top Posts" discovery panel integrated into the channel header context menu. When invoked, it provides a bounded scanning window (defaulting to 1000 messages with custom options) and extracts engagement metrics (Reaction Engagement, comment volume, and view counts) using a rate-limited, asynchronous scanner. 

Results are normalized, aggregated (grouping contiguous media albums and multi-part archives into unified Media Group entries), and persisted into a local database backed by a robust 3-layer eviction policy (Per-channel Top 300 quota, 30-day TTL, and 30-channel global LRU). Users can seamlessly switch between ranking by Reaction Engagement, comment count, and view count, filter by resource type (All, Documents only, Media only), and click any card to smoothly navigate and highlight the original post in the active chat view. Subsequent visits open instantly from local storage, and refreshes execute an Incremental Sync that fetches new posts while concurrently updating counters for existing cached items.

## User Stories

1. As a resource channel subscriber, I want to access a "Top Posts" option directly from the channel's top-right context menu, so that I can quickly discover the most popular content without manual hunting.
2. As a user browsing a channel, I want the Top Posts panel to slide out smoothly from the right side of the main window, so that my current reading context is preserved without opening a disruptive modal or separate window.
3. As a user visiting Top Posts for the first time in a channel, I want the client to scan a safe default window of 1000 recent messages, so that I get a rich historical sample within seconds without triggering API rate limits.
4. As a power user, I want the ability to customize the Scan Window depth (e.g. 500, 1000, 2000, or custom up to 5000), so that I can balance historical coverage with scanning time according to channel volume.
5. As a user performing a deep scan, I want to see a live visual progress indicator showing current messages fetched versus target and an immediate Cancel button, so that I always stay informed and in control.
6. As a user seeking high-quality software or books, I want to filter the scan to "Documents only", so that text-only announcements and chat chatter are omitted and the Scan Window reaches further back in time.
7. As a wallpaper or video enthusiast, I want to filter the scan to "Photos & Videos only", so that the resulting ranking is focused purely on visual media.
8. As a user evaluating overall popularity, I want posts ranked descending by Reaction Engagement (total sum of all reactions), so that I can see what the community has interacted with the most.
9. As a user interested in community discussion or troubleshooting, I want to sort posts descending by comment volume, so that I can locate active discussion threads, update notes, and community feedback.
10. As a user in a broadcast channel without a discussion group, I want the "Sort by Comments" toggle to remain visible but disabled with an informative tooltip, so that interface controls remain predictable without sudden layout shifts.
11. As a user interested in broad audience reach, I want to sort posts descending by view count, so that I can identify the most widely-seen announcements and posts.
12. As a user browsing resources published as multi-part archives or multi-photo sets, I want contiguous messages sharing a `grouped_id` aggregated into a single Media Group card, so that identical post segments do not flood the ranking.
13. As a user viewing a Media Group card, I want to see the total number of items in that set and aggregate reaction metrics, so that I understand the complete scope of the shared resource.
14. As a user returning to a previously scanned channel, I want the Top Posts panel to load instantly from local storage, so that I don't have to wait for redundant network fetches.
15. As a user refreshing an already-cached channel, I want an Incremental Sync to fetch new posts and refresh the reaction/comment counters of the existing Top 300 cached items in 1-2 network requests, so that old resources gaining fresh reactions stay accurately ranked.
16. As a user with limited disk space, I want each channel's cache capped at the top 300 entries and channels untouched for 30 days automatically evicted, so that local database storage remains negligible.
17. As a privacy-conscious user, I want a "Clear Channel Cache" button in the panel and an option in settings to clear all Top Posts caches, so that I have absolute control over retained metadata.
18. As a user when a post is deleted by channel admins, I want that post removed from my local Top Posts cache when the client receives the deletion update, so that stale deleted resources do not linger in the ranking.
19. As a user browsing the ranked card list, I want to click any card to smoothly scroll and highlight that message in the main channel timeline, so that I can immediately download the attachment or read the full context.
20. As a user encountering an API `FLOOD_WAIT` response during scanning, I want the client to pause gracefully, display a countdown notice, and allow resuming once the cooldown expires, so that my account is never put in jeopardy.

## Implementation Decisions

### 1. Domain Terminology
All modules and data models strictly adhere to the terms established in `CONTEXT.md`:
- `Top Post`: Local representation of a ranked channel message or aggregate.
- `Media Group`: Multiple contiguous messages sharing the same `grouped_id`.
- `Scan Window`: Bounded number of messages fetched during an analysis run.
- `Reaction Engagement`: Sum of all reaction counts across all emojis without bias.
- `Incremental Sync`: Dual-phase update (fetching new messages and batch-refreshing existing top posts).

### 2. Local Persistence Schema (`ChannelTopPost`)
A dedicated table inside the client's local SQLite storage with the following entity shape:
- Primary key ID
- Channel ID and Message ID
- Date and Scan Time (Unix timestamps)
- Total Reactions Count, Total Comments Count, Views Count
- Media Type enum (None, Photo, Video, Document, Grouped)
- Text Snippet (first 120 UTF-8 characters)
- Reactions Breakdown JSON string (mapping emoji identifier to count)
- Grouped ID (for Media Group aggregation)

### 3. Eviction & Retention Engine
- **Per-channel Cap**: Post-scan step executes a truncation retaining strictly the top 300 ranked items per channel.
- **TTL Expiration**: Records older than 30 days without an active user query are pruned during database startup.
- **Global LRU**: Active channel metadata count is capped at 30 channels. Excess channels are pruned by least-recent scan timestamp.
- **Deletion Hook**: Sync listener intercepts message deletion events and cascades removals to the local table.

### 4. Bounded Asynchronous Scanner
- Implemented as an asynchronous controller issuing batched MTProto message requests (`MTPmessages_Search` / `MTPmessages_GetHistory`) at 100 items per batch.
- Rate-limiting throttle: mandatory 150ms-250ms sleep between consecutive batch requests.
- Observable reactive stream emitting scanning state (idle, in-progress with percentage, paused-flood-wait, completed, aborted).

### 5. UI Architecture
- Subclassed from the client's native slide-out section framework (`SectionWidget`), matching existing history and shared-media inspectors.
- Top control bar housing:
  - Sort mode selector: Reactions / Comments / Views.
  - Resource filter selector: All / Documents / Media.
  - Action menu: Refresh (Incremental Sync), Rescan (configure Scan Window), Clear Cache.
- Scrollable card view with smooth thumbnail/icon rendering and double-buffered painting.
- Click delegate dispatching smooth timeline navigation and message highlight in the primary session controller.

## Testing Decisions

### Good Test Criteria
- Tests must assert observable external behavior, state transitions, and database queries rather than private implementation details.
- Scanning logic must be tested with simulated MTProto responses, verifying that rate-limiting delays, pagination boundaries, and cancellation signals work deterministically.
- Cache truncation and LRU eviction must be verified across boundary conditions (e.g. exactly 300, 301, and 0 items).

### Module Test Scope
- **Data & Eviction Tests**: Verify table creation, insert/upsert, Top 300 truncation, TTL purge queries, and LRU eviction order.
- **Scanner State Machine Tests**: Verify batching pagination, error recovery (flood wait simulation), and user abort handling.
- **Aggregation Logic Tests**: Verify that messages with matching `grouped_id` are combined into single entities with aggregated counters.

## Out of Scope

- Client-side unbounded full-channel historical crawling.
- Server-side indexing or relying on external scraping bots and cloud proxies.
- Automatic background scheduled polling of unviewed channels.
- Applying subjective sentiment analysis or weighting algorithms to emoji reactions (all reactions are counted equally).
- Cross-channel global aggregation or cross-channel ranking boards.
