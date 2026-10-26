# 1. Top Posts coverage as intervals, plus user-chosen scan ranges

Date: 2026-10-26

## Problem

A channel's Top Posts cache remembered a single number for "how far back has this
channel been scanned": the message count of the last scan, plus the id of the
oldest message it reached. That one number decided two things — whether the
"expand scan to N" action would run at all, and where it resumed from.

The shape was wrong in three ways.

It could only describe a prefix. Scanning the newest 5000 messages of a busy
channel reaches back a month or two, so a user who wants the best posts of a
specific past stretch — last spring, say — had no way to ask for it. A deeper
counter-based scan either hits the message budget or drags in a year of
unrelated history.

It was rewritten by every scan to whatever that scan counted, so a shallower
scan silently declared deep history unscanned.

It was also the resume point, so the wrongness compounded: expanding after a
shallower rescan restarted the deep walk from the shallow number.

## Decision

Coverage is a set of intervals per channel, not a single depth.

- Scanning from the newest message extends the interval that ends at the newest
  message. Scanning a scan range adds an interval of its own. A write only ever
  adds messages to coverage: no scan may remove history that was already
  scanned.
- "Expand scan to N" is an overlay operation: it does nothing when some
  interval already covers the newest N messages, and otherwise scans only the
  gap between the newest coverage and the requested depth.
- A scan range is a user-chosen pair of dates. The walk stops once it passes the
  older date, and the range's own progress row makes an interrupted range scan
  resume where it stopped instead of starting over.
- A range scan is bounded by its own message budget, independent of the
  counter-based window: the budget bounds one scan, the range decides what it
  visits.
- The cached post table keeps 3000 rows per channel while the panel lists 300,
  so widening coverage can bring an old high-scoring post into view without
  changing the list size.
- The panel can show a range view: the same cached posts restricted to the
  active range, with ranking mode and content filter still applied. Leaving the
  range restores the restricted-to-nothing list without rescanning.

## Consequences

- Answering "is depth N covered" reads the interval rows for one channel; there
  is no cached derivation to invalidate.
- A range scan costs one server batch per 100 messages of the range, so older
  ranges are slower; the scan is cancellable and resumes.
- A smaller rescan no longer resets the recorded depth, which removes the
  regression that motivated the change.
- Range scans and counter-based scans write the same post table, so a post found
  by either is visible to both views; only the view filter differs.

## Alternatives considered

- **Keep one depth counter and make "expand" always re-scan from the top** —
  simplest, but throws away existing coverage on every smaller scan and still
  cannot express "scan this past stretch".
- **Store only range rows and drop the counter-based depth** — loses the cheap
  default path where the newest messages are scanned without asking for dates.
- **Derive coverage from the cached post rows** — the post table is truncated to
  the newest 3000 rows, so it cannot answer how far back scanning reached.
- **Bound a range scan by the counter-based window** — a single past month of a
  busy channel exceeds 5000 messages, so the feature would silently truncate the
  very range the user asked for.
