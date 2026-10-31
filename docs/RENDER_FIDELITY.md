# In-game fidelity checklist — Mechanical Ore Washer

The mod's north-facing **static** model is located in
`assets/magneticraft2/models/multiblock/mechanical_ore_washer_north.json`.
It contains 66 Java model cuboids. Face aliases resolve to vanilla
`minecraft:block/stripped_spruce_log`, `minecraft:block/oak_planks`, and
`minecraft:block/copper_block`. This file contains only the static body, not
the complete in-world display.

The actual Forge block entity renderer adds, separately:

- A rotating drum (`multiblock/mechanical_ore_washer_rotor`)
- A small and large wooden pulley (existing Magneticraft pulley models)
- The moving leather drive belt drawn as geometry
- Conditional animated water levels and dirty-water color
- Input/output items and moving material

Studio does not import the Java logic that places and animates those elements.
The static example must be identified as **static only**, even after its real
PNG resources have been loaded. Showing a realistic block pattern is not the
same as successfully reproducing the Minecraft scene.

## What a texture import guarantees

- It uses PNG pixel data from a local asset source selected by the user.
- It honors `textures` aliases and imported per-face `uv` rectangles.
- It preserves those resources in editable Studio projects.
- It reports missing resource IDs and never fakes a replacement.

## Remaining differences from Minecraft

- Entity-side drum, pulley, belt, fluid and item rendering
- Minecraft lightmaps and biome/fluid tint
- Atlas mipmapping, animation metadata and tint indices
- Rotation/face-culling differences for unsupported model features

These limitations must stay visible until a side-by-side comparison with an
actual in-game screenshot confirms the full model matches.
