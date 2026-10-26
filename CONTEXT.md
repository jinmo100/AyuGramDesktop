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
The bounded slice of historical channel messages (e.g. 500, 1000) requested from the server to protect against rate limits; one request's size, not what a channel has actually covered.
_Avoid_: Crawl depth, fetch limit, scan range

**Coverage**:
The set of message intervals of a channel that have actually been scanned, each interval counted in messages of that channel. The intervals may be disjoint once a Scan Range is scanned, and they only ever grow: no scan removes history that was already covered. Coverage says nothing about how many Top Posts a scan produced.
_Avoid_: Loaded count, post count, cache size, 加载条数, scan depth

**Expand Scan**:
The action that raises a channel's Coverage to a requested depth by scanning only the not-yet-covered older messages. It does nothing when Coverage already meets the request, so repeating it is harmless.
_Avoid_: Rescan, reload, refresh, force rescan

**Scan Range**:
A user-chosen pair of publication dates that a scan visits instead of counting messages back from the newest one. It exists because a busy channel's message budget may only reach back a month or two, while the posts worth reading can be a stretch from last year. Scanning a range adds one Coverage interval and resumes from where it stopped if interrupted.
_Avoid_: Date filter, time filter, custom window, 时间范围

**Range View**:
The restriction of a Ranking Panel to Top Posts published inside the active Scan Range. It narrows the same cached set and leaves Ranking Mode and Content Filter in force; leaving the range restores the unrestricted list without rescanning.
_Avoid_: Date filter, ranged list, filtered channel

**Reaction Total**:
The unweighted sum of every reaction on a message or media group, whatever the emoji expresses; a reaction in neither classification list counts here and nowhere else.
_Avoid_: Reaction engagement, likes, upvotes, score

**Positive Reaction**:
A reaction whose emoji the user's positive list classifies as a sign of approval. A post carries its own count, ranked separately from the Reaction Total.
_Avoid_: Good reaction, like, upvote, quality score

**Negative Reaction**:
A reaction whose emoji the user's negative list classifies as a sign of derision or rejection. It is deliberately never subtracted from anything: the two counts stay independent so a heavily mixed post cannot pass for a clean one.
_Avoid_: Bad reaction, dislike, downvote, net score

**Ranking Mode**:
The single dimension an ordered list of Top Posts is sorted by — reactions, comments, views, positive reactions, or negative reactions. One mode at a time; switching it re-sorts the same cached set without rescanning.
_Avoid_: Sort type, order by, ranking method, 维度

**Content Filter**:
The restriction of a Top Post list to every post, to file-carrying posts only, or to media-carrying posts only. Independent of Ranking Mode: it narrows which posts are listed, never how they are ordered.
_Avoid_: Media type filter, post kind, category

**Ranking Panel**:
The left-column surface showing one channel's ordered Top Posts in place of the chat list. It keeps its channel, Ranking Mode, Content Filter and scroll position while the user opens a post or its comments.
_Avoid_: Top posts window, ranking dialog, side panel

**Incremental Sync**:
The synchronization cycle that fetches newly published messages while refreshing the reaction, comment and view counters of existing cached top posts.
_Avoid_: Quick refresh, partial reload, delta poll
