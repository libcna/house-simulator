# Clutter acquisition source (`HOUSE-00984`)

This capped group adds exactly the five shapes named by `docs/furnishing-kit.md`: a bin, suitcase,
toolbox, garden-tool set and paint tin. Generated `MODEL_PROP_KIT_STORAGE_BOX` instances replace a
separate crate acquisition, while the four completed fill-kit families supply pantry and shelf
items.

All five source GLBs are official 3D Assets CC0 1.0 releases:

* `bin.glb`: **Office waste bin**, asset 26268, *Office Lobby and Building Facilities*;
* `suitcase.glb`: **Suitcase**, asset 30311, *Mountain Border Checkpoint*;
* `toolbox.glb`: **Cantilever tool box**, asset 23944, *Hardware Store and DIY Warehouse*;
* `garden_tools.glb`: **Long handled tool bin**, asset 17752, *Garden Centre and Florist*;
* `paint_tin.glb`: **Paint tin, 5 litre**, asset 23929, *Hardware Store and DIY Warehouse*.

`tools/assets/clutter_prepare.py` pins the official CDN bytes, bakes each hierarchy through the
existing bounded static preparation path, adds UV0 and one enclosing twelve-triangle proxy, and
validates scale, origin and canonical material slots. The toolbox source includes open/close clips;
this static showcase derivative deliberately keeps its closed default pose and removes animation.
No interaction or inventory behaviour is introduced.
