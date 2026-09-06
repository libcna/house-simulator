# Rules for working in `cna-house`

*An identical copy of this file lives in `CLAUDE.md`. Both are kept in sync; read whichever your
tool loads, you are not missing anything by reading only one.*

The openeggbert build rules in `../CLAUDE.md` / `../AGENTS.md` apply here in full and are **not**
restated in detail. This file is the compliance checklist for them plus the rules that are
specific to this project.

---

## 0. The two authoritative documents

| Document | Authority |
|---|---|
| [`cna-house.md`](cna-house.md) | The architecture. Do not casually redesign it. |
| [`plan.md`](plan.md) | The task list and the execution ledger. Task ids are permanent. |

If implementation reveals a genuine contradiction or impossibility in either, investigate it, make
the **smallest** technically justified correction, record why, and never deviate silently.

---

## 1. The XNA-only rule — the one that is never negotiable

Runtime code may call:

* `Microsoft::Xna::Framework::*` — the XNA 4.0 API as implemented by CNA;
* `System::*` from sharp-runtime;
* the C++ standard library;
* its own code in the `cnahouse::` namespace.

Runtime code may **not** call, in any form:

`CNA::` anything · the CNAEXT engine layer (`CNA::Graphics::*`) · `CNA::Internal::*` ·
`ShaderEffect` · `PbrEffect` · `SkinnedPbrEffect` · `AvatarRenderer` · `SkinnedModelEXT` ·
`GraphicsDevice::SupportsCapability` or any other runtime capability query ·
`Model::getSkinsEXTProperty()` · `Model::setOwnedResources()` · any other `CNAEXT`-marked
convenience call · a `Model::Tag` read · CNA's `Graphics::SkinningData` / `AnimationClip` /
`Keyframe` / `AnimationPlayer` · OpenGL · OpenGL ES · EGL · Vulkan · WebGPU · Direct3D · Metal ·
SDL rendering · any native graphics handle.

**There is no allowlist and no middle tier.** `docs/xna-deviations.md` records the project-owned
(`cnahouse::`) subsystems and what XNA lacks; it grants permission to call nothing. If XNA 4.0
cannot do something, write it in `cnahouse::` or do without it. See
[ADR-0001](docs/decisions/ADR-0001-xna-only.md).

`CNA_CNAEXT=OFF` is forced by this project's CMake and must never be weakened. Offline tooling
(Blender, ffmpeg, `fxc`, Python, `cna-content`) is not runtime and is unconstrained.

Run `tools/ci/check_xna_only.py` before you commit. It will find you.

---

## 2. openeggbert build-rules compliance checklist

Tick every line before configuring any build. The reasoning — SSD endurance measured across ten
concurrent agents — is in `../AGENTS.md`; do not re-derive it, just comply.

- [ ] `CCACHE_DIR=/rv/cnaccache` and `CCACHE_BASEDIR=/rv` are exported. **There is exactly one
      ccache.** Never point `CCACHE_DIR` at a new directory, never set `CCACHE_MAXSIZE`.
- [ ] `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache` are passed (the
      `CMakePresets.json` presets do this; if you configure by hand, do it by hand).
- [ ] The build directory is one of the closed list — `build/`, `build-asan/`, `build-ubsan/`,
      `build-tsan/`, `build-probe/`, `build-consumer/` — **in this repository**. Never a new name,
      never a ticket, date, branch or topic suffix. Separate a ticket's work by **file-name
      prefix** inside the shared directory (`p1-skin`, `p0-result-check`), never by a new
      directory.
- [ ] A usable build directory does not already exist. Reconfigure only when the build is actually
      broken or the configuration genuinely changed.
- [ ] The target path is **not** under `/tmp` and not in the session scratchpad. Never run CMake,
      make, a compiler or `git clone` there. The scratchpad is for notes and short scripts.
- [ ] Third-party dependencies come from `~/deps/<name>`, never a fresh per-session clone.
- [ ] Parallelism is `-j$(nproc)`. RAM is the constraint, not cores: this machine has 30 GB, no
      swap, shared with other agents. Drop a single OOM-ing target to `-j2`, not the whole build.
- [ ] Sanitizer binaries are large. Build only the sanitizer variant the task needs.
- [ ] Probe binaries are deleted once their finding is written down. Never delete a build
      directory another session may be using — check `ps` and `/proc/*/cwd` first.

---

## 3. Project rules on top of those

**One task, one commit.** The commit message names the `HOUSE-00000` id, and `plan.md`'s checkbox
is ticked in the same commit. Definition of done: [`docs/workflow.md`](docs/workflow.md).

**Task ids are permanent.** Never renumber, never delete a historical task, never strike one
without recording why. New work takes the next free id **in its phase's reserved range**.

**Never mark a task complete while its acceptance criteria are knowingly unsatisfied.** If a task
is genuinely blocked, record the evidence in `plan.md` and move to independent work.

**No throwaway placeholders.** No empty classes, no methods that return hardcoded success, no
tests that assert `true`, no dead abstraction layers, no TODO-only systems presented as complete.
Scaffolding is fine when a task asks for scaffolding; it must still be real infrastructure.

**Do not modify CNA or sharp-runtime from this repository.** If you find a genuine CNA defect,
prove it, record it as a blocker in `cna-house.md` §6, and find a workaround here. CNA changes are
handled in CNA, in a separate session, under an explicitly approved upstream task.

**The house is data, not code.** Rooms, portals, lights, props and interactables live in
`assets-src/world/*.json`. C++ contains systems. Never hard-code a room.

**No third-party runtime dependency** is added. JSON is `System::Text::Json`; testing is
GoogleTest; physics, ECS, navigation and audio middleware are all rejected with reasons in
`cna-house.md` §82.3.

**Conventions are enforced, not suggested:** `docs/conventions.md` for ids, names and namespaces;
`.clang-format` for style; `-Wall -Wextra -Wpedantic -Werror` for the compiler.

**No massive asset downloads outside a task that asks for one.** Phase 0–3 work must not explode
the repository size. Never use an asset without provable provenance.

---

## 4. Before you finish

```bash
tools/ci/run_checks.sh          # layout, XNA-only, clang-format
git diff --check                # whitespace damage
git status                      # nothing accidental, no build products
```

Then verify: no CNA or sibling repository was modified; `CNA_CNAEXT=OFF` was not weakened; no
forbidden symbol entered a runtime source; `plan.md` is updated; one commit.
