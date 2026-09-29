# Changelog

Versions follow [`docs/versioning.md`](docs/versioning.md). The full release notes, known
limitations and credits of the final multiplatform release are `HOUSE-03077`'s.

## 1.0.0 — 2026-09-29 — the feature-complete Linux desktop release

The first tagged version: everything since the repository began on 2026-09-06, as the desktop
meets `plan.md`'s Definition of DONE (D1–D14 for the desktop; Web and Android follow in their own
milestones).

**The property.** A furnished house to walk through on foot: basement, ground floor, garage, two
upper floors and the attic, joined by real stairs, with the front and rear gardens, a shed and the
street. 96 cells, 90 of them accessible. Every accessible room is dressed to its purpose and lit
by day and at night; the twelve main rooms and five hero areas carry the most detail.

**Environment.** Time of day runs automatically (a 24-minute day by default) or is set; sun, moon
and stars follow the calendar; the sky shows clear, overcast and rain, with a snow state; rain
stays out of covered places, surfaces get wet and fog follows the weather. Interior lights follow
an automatic schedule.

**Audio.** Footsteps on six surface categories, an interior room tone, exterior day and night
ambience, and rain and wind layers that are quieter indoors, each with its own volume.

**Application.** Title, pause, one settings screen (graphics, audio, controls, environment),
credits with every third-party asset, quit and a controls hint. `C` starts or stops a bounded
filming tour of the whole property. Quality presets High, Web and Android (and Ultra), selectable
with `--quality` and in the settings; lower presets draw cooked vegetation LOD and upload only
their own level.

**Rendering.** Stock XNA 4.0 effects only (Tier S), with an optional compiled-effect tier (Tier E),
on CNA's OPENGLES3 renderer. No CNA extension is used; the XNA-only and strict-XNA gates enforce
it.

**Performance.** The eight fixed scenarios meet the High target on the reference desktop (CPU
≤ 9.5 ms, GPU ≤ 14 ms, ≤ 1,800 draws, ≤ 3.4 M triangles); the Web and Android presets are measured
against their own budgets.

**Quality.** Unit, GPU integration and software render suites pass, also under ASAN and UBSAN;
a 20-minute and one 2-hour stability run pass; the static gates guard the content and code.

**Packaging.** `tools/ci/package_linux.py` builds a self-contained Linux archive (game, CNA and SDL
libraries, content, licences, launcher) that runs on a clean profile with or without an audio
device.

**Known limitations.** Web and Android are not yet released. Two logged S3 items remain: mottled
light on the attic storage wall and the plain street planting.
