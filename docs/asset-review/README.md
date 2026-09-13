# Hero-asset review register

This directory is the durable visual-approval record required by `cna-house.md` §19.3. The four
hero categories were registered by `HOUSE-00298` before their final runtime assets existed so the
later authoring tasks cannot silently bypass the quality bar.

| Hero | Review | Current state | Completion owner |
|---|---|---|---|
| Human avatar | [`human-avatar/review.md`](human-avatar/review.md) | prepared | `HOUSE-02131` |
| Dog | [`dog/review.md`](dog/review.md) | prepared | `HOUSE-02041` |
| Cat | [`cat/review.md`](cat/review.md) | prepared | `HOUSE-02091` |
| Player car | [`player-car/review.md`](player-car/review.md) | prepared | `HOUSE-01035` |

## State transition

`prepared` is an acceptance contract, not a verdict. The named owner replaces each explicitly
missing path or value with measured evidence, renders the four views at final scale in actual scene
lighting, records the reference's provenance, and checks every applicable item. It then moves the
record to `ready for review`. A human reviewer supplies their name, ISO date and verdict and moves
it to `complete`; an author may not approve their own asset.

An `approved with changes` verdict is complete only after the required changes are identified and
the review images show the changed asset. `rejected` is a completed review, but it does not permit
the asset to enter the game. The manifest's `review.status` may say `approved` only when it points
to a `complete` record whose verdict permits use.

The four model-specific records deliberately do not invent manifest ids, output paths, reviewer
names or dates for assets that have not been authored. That absence is executable work owned by
the task named in each record, rather than a false sign-off.
