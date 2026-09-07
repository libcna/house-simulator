# Mixamo — licence evidence

`HOUSE-00271`. The plan's stated expectation was "not used for shipped files". That expectation is
**confirmed**, and this file records why in a form that will still answer the question in a year.

| | |
|---|---|
| FAQ | <https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html> |
| Operator | Adobe |
| Retrieved | 2026-09-07 |
| **Verdict** | **REJECTED for shipped files.** Usable as a private reference during evaluation only. Nothing depends on it. |

---

## What the FAQ actually grants

> Mixamo is available free and does not require any additional purchases or subscriptions.
>
> You can use both characters and animations **royalty free for personal, commercial, and non-profit
> projects** including:
> * Incorporate characters into illustrations and graphic art.
> * 3D print characters.
> * Create films.
> * **Create video games.**

That is a genuine, broad grant for **use in an end product**, and "create video games" is this
project's case exactly. If `cna-house` shipped only a compiled binary, Mixamo would be usable.

## Why it is rejected anyway

**The grant is about use, and says nothing about redistribution.** Every item in that list is an end
product — an illustration, a print, a film, a game. None of them is "publish the animation file".
`assets-src/` is a **published** directory, so committing a Mixamo `.fbx` there distributes the
animation data as data, which the FAQ neither permits nor discusses.

ADR-0012's rule for a permission that is not granted in terms is that we do not assume it. This is
the fourth source in this phase with the same shape — Quaternius, CMU, BlenderKit Royalty Free — and
the reason is always the same: **this project publishes its asset sources, so "may I use it?" and
"may I republish it?" are different questions and only the second one matters for `assets-src/`.**

There is a further wrinkle that distinguishes Mixamo from CMU, where the derivative *was*
shippable. With CMU the asset is a raw capture and our deliverable is a retargeted `.chanim` — a
genuine derivative work. With Mixamo the animation *is* the finished product; a `.chanim` of a
Mixamo clip is substantially the same data in another container, which is much closer to
"redistributing in converted form" than to deriving something new.

Two secondary points:

* Mixamo requires an **Adobe account**, and access is governed by Adobe's General Terms of Use
  rather than by an asset licence. Those terms are not reproduced in the FAQ, can change, and are
  not something this project can archive as a stable licence text the way it archives OFL or CC0.
* The service is **not available in every territory** (the FAQ excludes Enterprise/Federated IDs and
  users with a China country code), which is an odd dependency for a shipped game's content to have
  even indirectly.

## Consequence: none

`HOUSE-00295` sources locomotion from the **CMU Motion Capture Database** (`HOUSE-00270`), whose
terms expressly permit including the data in commercially-sold products and whose retargeted
derivatives are shippable. Mixamo was a convenience, not a dependency.

**Permitted use here:** as a private reference while evaluating rigs — looking at how a Mixamo clip
solves a foot-plant, for instance. Nothing from Mixamo enters `assets-src/`, the manifest, or a
build.
