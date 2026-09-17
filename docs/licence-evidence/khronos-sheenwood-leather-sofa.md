# SheenWoodLeatherSofa licence evidence

- Asset: `SheenWoodLeatherSofa`
- Final publisher/copyright holder: Darmstadt Graphics Group GmbH
- Source copyright record: `© 2024 Darmstadt Graphics Group GmbH, CC BY 4.0
  International, changes by Eric Chadwick. Original model by Fran Calvente, CC0
  Polyhaven.com`
- Publisher: Khronos Group glTF Sample Assets
- Source page:
  <https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/SheenWoodLeatherSofa>
- Pinned binary:
  <https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/SheenWoodLeatherSofa/glTF-Binary/SheenWoodLeatherSofa.glb>
- Pinned source size: 10,107,912 bytes
- Pinned source SHA-256:
  `5349e042ad41e695e89f1110230c4ee0c75b2bc62ef830c7016be6ecf665bfb6`
- Licence: Creative Commons Attribution 4.0 International (`CC-BY-4.0`)
- Archived legal code: [`licenses/cc-by-4.0/LICENCE.txt`](../../licenses/cc-by-4.0/LICENCE.txt)

The binary's own copyright field is the authority for the complete author/change chain above.
The sibling `living-room-simulator` independently pins the same delivery at the same hash and
records Darmstadt Graphics Group GmbH and CC BY 4.0; no sibling runtime or CNAEXT code is copied.

`tools/blender/living_sofa_prepare.py` welds duplicated source corners before decimation, scales
the model to the publisher catalogue's 2.20 m fit, produces LOD0/LOD1 and a collision proxy,
removes embedded maps and unsupported runtime extensions from the GLB, then extracts five authored
base-colour maps as deterministic PNGs no larger than 256 px. The distributed manifest attribution
identifies Darmstadt Graphics Group, Eric Chadwick, Fran Calvente / Poly Haven, the licence and
these modifications. No publisher preview render is packaged.
