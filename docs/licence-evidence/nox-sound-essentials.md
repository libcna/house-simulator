# NOX Sound "Essentials Series" — licence and identity verification

`HOUSE-00276` (Q-02). The collection at `/rv/tmp/Essentials_Series_NOX_SOUND/` is the backbone of
this project's audio: `HOUSE-00277` selects roughly 430 files from it, and `HOUSE-00280` maps its
footstep packs onto the game's 20 surfaces.

Planning declared it CC0 on the strength of the README bundled with the download. That is a
first-party claim travelling with the files, but a bundled README proves nothing about **which**
collection is on this disk. This task had to answer two separate questions:

1. does the publisher release the Essentials Series under CC0?
2. is the copy in `/rv/tmp/` actually that release?

| | |
|---|---|
| Publisher | Nox_Sound / Nox_Sound_Design — an independent sound designer |
| Authoritative page | <https://nox-sound-design.itch.io/essentials-series-sfx-nox-sound> |
| Retrieved | 2026-09-07 |
| Licence | **CC0 1.0**, archived at `licenses/cc0-1.0/LICENCE.txt` |
| **Verdict** | **CONFIRMED — CC0, verified, for 1 634 of the 1 644 files.** Ten files are excluded; see §4. |

---

## 1. Does the publisher release it under CC0?

The publisher's own itch.io product page states, verbatim:

> **Essentials Series - Free Sound Effect**
>
> Hi, I'm an independent sound designer for video games. I publish the Essentials Series (1,644 free
> sound effects) to help creators focus on their projects and creative work.
>
> […]
>
> **You can use these sounds for personal and commercial projects. All sounds are released under
> CC0, allowing you to use them freely without attribution or restrictions.**

That is the rights holder, on their own storefront, naming the licence, the permitted uses and the
absence of attribution obligations. It is the authoritative statement ADR-0012 asks for.

**Finding it required following the publisher's own links rather than trusting the bundled README.**
The README PDF names four channels — Instagram, A Sound Effect, Freesound, Unity — and *none of them
is where the Essentials Series lives*. The route was: README PDF → the publisher's A Sound Effect
page (403 to automated requests; read via the Wayback Machine) → `linktr.ee/Nox_Sound` → the itch.io
page. Recorded because the obvious first-party pages give the *wrong* answer, as §3 explains.

## 2. Is the local copy that release? — identity, not folder names

Folder names prove nothing; this was checked by counting.

The publisher states **1,644** sound effects. The local tree contains **exactly 1 644 `.wav`
files** (plus 18 non-audio files: the README, pictures, a datasheet and a `.url` shortcut). The
per-sub-collection counts line up too:

| Sub-collection | Publisher's page | Local `.wav` count |
|---|---|---|
| Footsteps – Essentials | 479 | **479** |
| Vehicle – Essentials | 161 | **161** |
| Nature – Essentials | 18 | **18** |
| São Miguel – Flows | 14 | **14** |
| Iceland – Footsteps + Flows | 210 + 23 = 233 | **233** |
| Electromagnetic | 71 | 72 |
| Voices – Essentials | 526 | 657 |
| *(Sample_A_Sound_Effect)* | not itemised | 10 |
| **Total** | **1 644** | **1 644** |

Five of the seven itemised packs match exactly, and **the headline total matches exactly**. The two
that differ are both *larger* locally, which is consistent with the page's per-pack prose being
stale against its own headline figure: the listing says "Updated 19 days ago" and carries a devlog
entry "Essentials Series - Update #1". The publisher also confirms in the page's comments that the
pack is still being extended.

The download is named `Essentials_Series_NOX_SOUND.zip` (988 MB), which is the local directory's
name, and 988 MB compressed is consistent with 1.1 GB of 24-bit WAV uncompressed.

An unrelated collection matching the exact headline count *and* five exact per-pack counts *and* the
archive name is not a coincidence worth entertaining. **Identity is established.**

## 3. What the other channels would have told us, and why they are wrong here

This is the part that makes the verification worth its cost.

**A Sound Effect** (<https://www.asoundeffect.com/sounddesigner/nox-sound/>) sells NOX libraries as
**paid commercial products** — "Foley – Household Essentials" (741 sounds, $20), "Ambiance – Nature"
(104 sounds, $20), "Iceland – Footsteps (Supporter Pack)" ($10), and others, under A Sound Effect's
own License Agreement. Nothing there is CC0.

**Freesound** (<https://freesound.org/people/Nox_Sound/>) carries 12 packs, all showing "Creative
Commons 0" — but they are a *sampler*. The Freesound "Pack - Electromagnetic" holds **5** sounds
against the local 72; "Pack - Footsteps" holds **12** consolidated sequence files against the local
479 individual ones. Only 2 filenames match exactly between the local Electromagnetic set and the
Freesound pack.

**Unity Asset Store** (publisher 52638) distributes under Unity's own store terms.

So the publisher operates a mixed model: free CC0 releases *and* paid commercial libraries of
overlapping subject matter, sometimes overlapping recordings. **"NOX Sound is CC0" is false as a
general statement.** The CC0 grant attaches to the Essentials Series specifically, which is why this
document records the exact release and the exact file count rather than the publisher's name.

Had this task stopped at the Freesound account — the obvious first-party CC0 evidence — it would
have "verified" CC0 for about 60 sounds and silently implied it for 1 644.

## 4. The ten files that are excluded

`Sample_A_Sound_Effect/` contains 10 `.wav` files and one Windows `.url` shortcut pointing at
<https://www.asoundeffect.com/sounddesigner/nox-sound/>. Their names —
`Household_Door_Wood_Open_Stereo.wav`, `Cloth_Coat_PickUp_Stereo.wav`,
`Backpack_Medium_Polyester_Drop_Stereo.wav`, `Ambiance_Nature_Rain_Calm_Leaves_Loop_Stereo.wav`,
`Atmosphere_Drone_Hum_Eerie_Loop_Stereo.wav` and similar — map onto NOX's **paid** A Sound Effect
libraries (*Foley – Household Essentials*, *Foley – Clothes Movement*, *Foley – Backpack*,
*Ambiance – Nature*, *Ambiance – Atmosphere*). They are promotional teasers for products that are
sold, shipped inside a bundle whose page says "All sounds are released under CC0".

A rights holder may licence their own recordings twice, so this is not a contradiction — but it is
not *clear* either, and ADR-0012's rule for unclear terms is that the asset is unusable until
resolved.

**Decision: `Sample_A_Sound_Effect/` is excluded from the shippable subset.** It costs 10 files out
of 1 644, in categories (`door`, `cloth`, `ambience`) that `HOUSE-00282` and `HOUSE-00290` source
separately anyway. `HOUSE-00277` must exclude the directory explicitly, alongside the exclusions it
already names.

## 5. Consequences for the tasks downstream

* **`HOUSE-00277`** — the selectable pool is 1 634 files, not 1 644. Its existing exclusions (the
  Azores flows, the combat voices, the truck pack) stand; `Sample_A_Sound_Effect/` is added.
* **`HOUSE-00279`** — every imported row records `CC0-1.0`, `licenceFile: licenses/cc0-1.0/LICENCE.txt`,
  `url: https://nox-sound-design.itch.io/essentials-series-sfx-nox-sound`, `retrieved: 2026-09-07`,
  `author: Nox_Sound`, and both the original and converted hashes. Attribution is not required and
  the credits generator omits it for CC0.
* **`HOUSE-00193`** — the publisher states the collection is **48 kHz / 24-bit**, with São Miguel –
  Flows at **96 kHz**. That is measured independently in §6 rather than taken on trust, and it is
  the evidence behind the correction that 48 kHz must not be blindly resampled to 44.1 kHz.
* The collection lives in `/rv/tmp/`, which is **not** version-controlled and is on a volume this
  project's own build rules treat as scratch. Only the selected, converted subset enters
  `assets-src/`. The originals' hashes in the manifest are what make the conversion auditable once
  the source directory is gone.

## 6. Measured formats

See `docs/licence-evidence/nox-sound-format-census.txt` for the full per-file census produced by
`ffprobe` over all 1 644 files, and the summary recorded with `HOUSE-00276` in `plan.md`.
