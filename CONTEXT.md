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

**Reaction Total**:
The unweighted sum of every reaction on a message or media group, whatever the emoji expresses; a reaction in neither classification list counts here and nowhere else.
_Avoid_: Reaction engagement, likes, upvotes, score

**Positive Reaction**:
A reaction whose emoji the user's positive list classifies as a sign of approval. A post carries its own count, ranked separately from the Reaction Total.
_Avoid_: Good reaction, like, upvote, quality score

**Negative Reaction**:
A reaction whose emoji the user's negative list classifies as a sign of derision or rejection. It is deliberately never subtracted from anything: the two counts stay independent so a heavily mixed post cannot pass for a clean one.
_Avoid_: Bad reaction, dislike, downvote, net score

**Incremental Sync**:
The synchronization cycle that fetches newly published messages while refreshing the reaction, comment and view counters of existing cached top posts.
_Avoid_: Quick refresh, partial reload, delta poll
