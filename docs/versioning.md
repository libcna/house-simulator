# Versioning

## The scheme

```
MAJOR.MINOR.PATCH+gHASH
0.7.2+g1a2b3c4
```

| Component | Meaning |
|---|---|
| `MAJOR` | `0` until the feature-complete desktop version (`cna-house.md` §79). It becomes `1` there and thereafter increments only on a change that invalidates saves beyond what a migration can repair, or that drops a platform. |
| `MINOR` | A phase of `plan.md` reaching its exit criterion. Features arrive here. |
| `PATCH` | Fixes and content updates that add no feature. |
| `+gHASH` | Build metadata: the short git hash of the commit built. Appended automatically, never authored. A dirty working tree appends `.dirty`. |

This is SemVer's grammar with build metadata, deliberately: `+gHASH` does not participate in
precedence, so two builds of the same tagged version compare equal even though we can still tell
them apart in a bug report.

## Where the version string lives

**`VERSION`, in the repository root, is the single source of truth.** It contains exactly one line,
`MAJOR.MINOR.PATCH`, and nothing else — no prefix, no metadata, no trailing text — so that CMake,
Python tooling and a shell script can all read it without a parser.

Everything else derives from it:

| Consumer | How it gets the version |
|---|---|
| CMake | `file(READ VERSION)` at configure time, feeding `project(cna-house VERSION ...)` |
| The binary | `include/cnahouse/app/Version.hpp`, **generated into the build directory** by `configure_file`, exposing `cnahouse::app::kVersion`, `kGitHash` and `kVersionFull` as `constexpr std::string_view` |
| The save file | `payload.gameVersion`, written as the full `MAJOR.MINOR.PATCH+gHASH` string ([ADR-0008](decisions/ADR-0008-save-format.md)) |
| The window title and the HUD | `kVersionFull` in debug builds; `kVersion` in release |
| `--version` | prints `kVersionFull`, the configured renderer and the resolved render tier |
| Packaging | the archive name |

The generated header lives in the **build** directory, never in `include/`: it is build output, and
committing it would create a second source of truth that drifts.

`kGitHash` comes from `git describe --always --dirty=.dirty --exclude='*'` at configure time. When
git is unavailable — a source archive, a clean-room build — it is `unknown`, and the version string
is simply `MAJOR.MINOR.PATCH`. A missing hash never fails a build.

## Releasing

1. Update `VERSION`.
2. Update `CHANGELOG.md`.
3. Commit as `release: 0.7.2`.
4. Tag `v0.7.2`.

The tag is `v`-prefixed; the `VERSION` file is not. Nothing reads the tag at build time, so a
forgotten tag is a bookkeeping mistake rather than a broken build.

## What is versioned separately

Three things carry their own version numbers on purpose, because they change on different
schedules from the program:

| Artefact | Version | Owner |
|---|---|---|
| Save file | integer `version`, with a migration chain | [ADR-0008](decisions/ADR-0008-save-format.md) |
| World data | `"schema": "cna-house/<kind>/<n>"` per file, plus a `worldHash` over the set | [ADR-0005](decisions/ADR-0005-data-driven-world.md) |
| Asset manifest | `"schema": "cna-house/assets/1"` | [ADR-0012](decisions/ADR-0012-asset-licensing.md) |

A change to any of them may happen without a `MINOR` bump, and a `MINOR` bump does not imply any of
them changed.
