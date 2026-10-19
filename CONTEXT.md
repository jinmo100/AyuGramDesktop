# AyuGram Desktop Top Posts Context

Provides domain vocabulary for the bounded scanning, multi-dimensional ranking, and local caching of popular channel messages in AyuGramDesktop.

## Language

**Top Post**:
A channel message or media group evaluated by reaction count, comment volume, or view count, cached locally for discovery.
_Avoid_: Best message, hot post, pinned post

**Media Group**:
A set of contiguous messages sharing the same `grouped_id` published together as an album or split archive, aggregated as one logical entry.
_Avoid_: Batch messages, message bundle, cluster

**Scan Window**:
The bounded slice of historical channel messages (e.g. 500, 1000) requested from the server to protect against rate limits.
_Avoid_: Crawl depth, fetch limit, scan range

**Reaction Engagement**:
The unweighted total sum of all emoji reactions on a message or media group, treating all reaction types as valid engagement.
_Avoid_: Likes, upvotes, positive score

**Incremental Sync**:
The synchronization cycle that fetches newly published messages while refreshing the engagement counters of existing cached top posts.
_Avoid_: Quick refresh, partial reload, delta poll
