# Interior lamp acquisition source (`HOUSE-00981`)

The project already has reusable table, floor, ceiling, pendant and task-puck fixtures. Following
rule R8, this group adds only the two shapes still named by `docs/furnishing-kit.md`, within the
four-model cap:

* `desk_lamp.glb` derives from **Desk Lamp Task**, asset 38798 in the official 3D Assets
  *Bedroom and Living Room Furniture* pack. The pack reports Muse Spark via T3 Code as the AI
  generator and publishes the model under CC0 1.0.
* `utility_ceiling_light.glb` derives from **Workshop ceiling light bar, 2.4 m**, asset 34238 in the
  official 3D Assets *Woodworking Workshop and Joinery* pack. The pack reports Claude Opus 5 as the
  AI generator and publishes the model under CC0 1.0.

`tools/assets/lamp_fixtures_prepare.py` pins both official CDN files by SHA-256, bakes their static
geometry and hierarchy through the existing bounded preparation path, adds UV0 and one enclosing
collision proxy, and validates scale and origin. The desk fitting rests on its support origin; the
utility fitting keeps its authored top fixing face at the ceiling origin. Canonical warm/neutral
emissive material roles mark the physical diffuser surfaces for M5's existing light groups. There
is no lamp interaction, lighting manager or other runtime system.
