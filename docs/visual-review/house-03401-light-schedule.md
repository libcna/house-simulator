# HOUSE-03401 automatic-light schedule evidence

Date: 2026-09-24

## Authored coverage

`layout.lights.json` assigns exactly one schedule row to each of its 138 light groups. The bounded
class distribution is 29 `SC-OFF`, 18 `SC-LIVING`, 16 `SC-BED`, 18 `SC-WET`, 18 `SC-TASK`, 26
`SC-CIRC` and 13 `SC-DUSK`. The authored-world validator rejects an unknown, duplicate or missing
group assignment, so a later fixture group cannot silently fall outside the schedule.

The seven classes reuse the existing lighting system and shared sun/clock. Each group receives a
stable FNV-derived offset in the inclusive range -8 to +8 minutes. Living, bedroom, wet, task and
circulation windows require solar altitude at or below -4 degrees; dusk groups follow that solar
threshold alone; off groups stay off. The existing per-fixture dusk staggering remains intact.
`--light-on` and `--light-off` establish a persistent override after startup.

## Night review

Round 146 captured the six fixed `Z-L1` views at 22:00 in clear weather with neither
`--light-on` nor `--light-off`. The contact sheet and full-resolution frames show scheduled output
in the first-floor hall and landing, master bedroom and bathroom, second bedroom and front balcony.
The fixtures do not switch as one block because their group offsets differ. The review proves the
schedule is active; it does not claim final C3 room luminance, which remains the bounded zone work
of `HOUSE-03403` through `HOUSE-03407`.

Local, Git-ignored evidence: `captures/house-03401-schedule-r146/`.

## Automated evidence

- `LightScheduleTests.*`: exact time windows, deterministic bounded offsets, a representative
  group in B1/L0/L1/L2/L3 changing its room's artificial result, and persistent overrides.
- `validate_world.py`: exact schedule coverage and schema vocabulary.
- The complete unit suite and static gates are recorded in the task note and handoff.
