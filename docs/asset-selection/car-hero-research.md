# Player-car hero asset research

`HOUSE-00293`, retrieved 2026-09-13. The fixed target comes from `cna-house.md` §19.3, §70.5,
§72 and R-03: a realistic hero car, 4.2–5.2 m long, 1.7–2.0 m wide and 1.4–1.9 m high, at no
more than 45,000 LOD0 triangles. It is a static prop, not a vehicle simulation, and is seen in a dim
garage from at most 6 m. It still needs a convincing silhouette, plausible exterior and visible cabin,
clean normals and winding, LODs, a collision proxy, geometry suitable for the generated snow shell
and the four-view hero sign-off.

This task fixes the final design as an **unbranded contemporary five-door family estate**, 4.65 m
long, 1.84 m wide and 1.48 m high. That ordinary form fits the family's large garage without
introducing any real manufacturer's marks or a conspicuous sports-car story. The wheels, doors and
car itself do not move; the brief explicitly defers a drivable car.

No candidate below cleared provenance, appearance and budget together, so no third-party bytes
were downloaded and no manifest row was created.

## Attempt 1 — Sketchfab, CC0-filtered search

The strongest genuinely author-declared CC0 result was
[Concept Car 037](https://sketchfab.com/3d-models/free-concept-car-037-public-domain-cc0-646a3a31b0224379b6e767abc34d58dd).
Its author links making-of evidence, but the model is a cartoon/futuristic concept at **198,300
triangles**, 4.4 times the fixed car budget. Similar CC0 results from the same author reach 236,300
and 300,700 triangles. Decimating that far would be a new topology job around glazing, wheel arches
and panel gaps, while still leaving the wrong design.

**Result: rejected.** It misses the ordinary realistic silhouette and exceeds the triangle budget
by more than §19.3's three-times rejection threshold. Sketchfab is also an uploader-led aggregator
that [`sketchfab.md`](../licence-evidence/sketchfab.md) excludes from hero assets; a CC0 metadata
label alone cannot promote it to the chosen player car.

## Attempt 2 — Blend Swap, CC0-filtered search

The closest detailed CC0 candidate was
[1968 Mustang GT Fastback](https://blendswap.com/blend/16609). Its page calls it a high-poly car
with a fully modelled interior and says it was modelled after the branded car in the film *Bullitt*.
It gives no triangle count or LODs and supplies a Cycles `.blend`, not a game-ready mesh. It is also
a conspicuous historic sports car rather than the ordinary contemporary family car the garage
needs.

Other prominent CC0 results are likewise named replicas or overt muscle/sports cars. Choosing one
would exchange the modelling task for uncertain brand/design provenance and an unmeasured
retopology task. Blend Swap expressly warrants no third-party rights, as archived in
[`blendswap.md`](../licence-evidence/blendswap.md), and the project's standing source verdict says
never to use it for a hero asset.

**Result: rejected.** A visually attractive uploader-declared CC0 model is not enough: the source
cannot establish a clean chain of title for the branded design, the game budget is unproved, and
the silhouette tells the wrong story.

## Attempt 3 — Quaternius

The first-party [Cars Pack](https://quaternius.com/packs/cars.html) contains eight textured car
models in FBX, OBJ and Blend formats. They are useful low-poly scenery cars, but their deliberately
faceted style lacks the panel, lamp, tyre, glazing and cabin detail needed when the player walks
within arm's reach. They are better candidates for `HOUSE-00847`'s distant neighbourhood traffic
than for the car in the garage.

There is an independent legal failure. The pack page and current FAQ label models CC0, while the
current [Quaternius Asset License v1.0](https://quaternius.com/license.html) says that neither the
original nor a modified asset may be redistributed as an asset. This public repository publishes
`assets-src/`, including its modified LODs and source model. The contradiction is archived in
[`quaternius-kenney-polypizza.md`](../licence-evidence/quaternius-kenney-polypizza.md), and the
project has no copy proven to have been acquired under the pack's 2018 CC0 terms.

**Result: rejected.** The models fail the close-up visual bar, and the source's contradictory
current terms cannot support publishing source and derived files here.

## Decision — build it ourselves

No sourced model passes the fixed bar. Risk R-03's fallback is selected. Unlike the animal phases,
the permanent ledger previously had only `HOUSE-00998`'s room-furnishing task, not a task that could
honestly own a hero model. The next free phase-13 ids therefore schedule the missing work:

1. `HOUSE-01035`: author the unbranded 4.65 × 1.84 × 1.48 m estate in Blender, including the
   exterior and simplified visible cabin, then make LOD1/LOD2 and collision meshes and complete the
   four-view sign-off;
2. `HOUSE-01036`: export, manifest, validate and build the car through the real `.glb`/`.cnb`
   pipeline, proving scale, origin, topology, LOD and 45,000-triangle limits;
3. `HOUSE-00778`: generate the car's snow shell from the verified source geometry;
4. `HOUSE-00998`: place that verified asset while furnishing `L0_GARAGE`.

The source mesh, materials and textures will be project-authored. This avoids a branded replica,
keeps the static-prop scope explicit and gives the player car a real acquisition/build path without
hiding it inside a 55-prop room task.
