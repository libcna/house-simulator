"""Authored stock-XNA material ids for unbaked outdoor static geometry.

The source role remains a descriptive glTF material *name*; `materialId` in glTF extras and
`chunks.bin` is the stable canonical identity. Outdoor geometry has a Basic vertex layout because
it has no per-room lightmap bake. Its material row must therefore also request BasicEffect.
These mappings are offline content-tool choices, never runtime room/material-name guesses.
"""

ROLE_IDS = {
    "TERRAIN_asphalt": "MAT_OUTDOOR_ASPHALT",
    # A separate source role lets terrain integrity tests distinguish the raised render-only
    # driveway joints from the physical asphalt field; both truthfully bind the same finish.
    "TERRAIN_asphalt_detail": "MAT_OUTDOOR_ASPHALT",
    "TERRAIN_bluestone": "MAT_OUTDOOR_BLUESTONE",
    "TERRAIN_bluestone_detail": "MAT_OUTDOOR_BLUESTONE",
    "TERRAIN_concrete": "MAT_OUTDOOR_CONCRETE",
    "TERRAIN_grass": "MAT_OUTDOOR_GRASS",
    "TERRAIN_lawn_worn": "MAT_OUTDOOR_LAWN_WORN",
    "TERRAIN_gravel": "MAT_OUTDOOR_GRAVEL",
    "TERRAIN_soil": "MAT_OUTDOOR_SOIL",
    "TERRAIN_mulch": "MAT_OUTDOOR_MULCH",
    "FENCE_board": "MAT_OUTDOOR_FENCE_BOARD",
    "FENCE_ornamental": "MAT_OUTDOOR_FENCE_METAL",
    "GATE_board": "MAT_OUTDOOR_FENCE_BOARD",
    "GATE_ornamental": "MAT_OUTDOOR_FENCE_METAL",
    "GARDEN_bed": "MAT_OUTDOOR_GARDEN_WOOD",
    "GARDEN_compost": "MAT_OUTDOOR_GARDEN_WOOD",
    "GARDEN_stone": "MAT_OUTDOOR_BLUESTONE",
    "GARDEN_trellis": "MAT_OUTDOOR_GARDEN_WOOD",
    "ROAD_grate": "MAT_OUTDOOR_ROAD_GRATE",
    "ROAD_paint": "MAT_OUTDOOR_ROAD_MARKING",
    "SHED_timber": "MAT_OUTDOOR_GARDEN_WOOD",
    # HOUSE-03267 finishes the existing generated shed with material families already used by the
    # house shell. Clear glass gets the same generator-owned unbaked variant as those opaque
    # families; no new texture, shader or runtime path exists.
    "SHED_siding": "MAT_OUTDOOR_SIDING",
    "SHED_trim": "MAT_OUTDOOR_SOFFIT",
    "SHED_roof": "MAT_OUTDOOR_ROOF",
    "SHED_door": "MAT_OUTDOOR_FENCE_METAL",
    "SHED_glass": "MAT_OUTDOOR_GLASS",
}

# House-shell source slots already carry real base ids. When the shell is unbaked outdoors,
# compile a BasicEffect variant with the same approved albedo; baked interior uses the base id.
UNBAKED_VARIANTS = {
    "MAT_BLUESTONE_PAVER": "MAT_OUTDOOR_BLUESTONE",
    "MAT_BRICK_WATER_TABLE": "MAT_OUTDOOR_BRICK",
    "MAT_CONCRETE_BROOM": "MAT_OUTDOOR_CONCRETE",
    "MAT_DECK_WOOD": "MAT_OUTDOOR_DECK",
    "MAT_ROOF_SHINGLE": "MAT_OUTDOOR_ROOF",
    "MAT_SIDING_WARM_WHITE": "MAT_OUTDOOR_SIDING",
    "MAT_SOFFIT_WHITE": "MAT_OUTDOOR_SOFFIT",
}
