# Studio editor experience — v0.5 direction

The v0.4 build proved that C++, Qt and OpenGL could draw cubes, transform them,
and display texture assets. It did **not** yet feel like an editor. This iteration
focuses on the actual authoring workflow rather than increasing the feature count.

## The job Studio must solve

1. Author a **build layout**, including controller and inputs/outputs.
2. Author the **formed visual model** at arbitrary Minecraft-block scale.
3. Inspect/select/rename/organize components without fighting panels.
4. Import existing Java Blockbench/Minecraft model geometry as editable cuboids,
   then assign the mod's PNG textures and correct UVs.
5. Keep all editable sources portable in a Studio document.

## Workspace contract

- **Build** presents the construction layer editor and its palette. Geometry tools
  are hidden; the selected layer and controller location remain visible.
- **Model** gives priority to the 3D canvas, a vertical tool rail and searchable
  Outliner. Model creation and navigation are always one action away.
- **UV / Texture** remains attached to the *same* formed model and selection,
  surfacing face/texture tools instead of creating a separate file/document.
- The Object Inspector is contextual to the selected model part. Model geometry
  can be edited both in the viewport and with precise 1/16-block coordinates.
- Undo/redo, project save, and a true **frame selected** action work across workflows.

## Ore Washer acceptance exercise

The shipped demo is **not** the current real Ore Washer; it is an editable
historical proof-of-concept. The acceptance test must therefore use the actual
Forge resource files, not the demo:

1. Open the 3x2x3 Ore Washer construction JSON via File > Open.
2. Switch to Model > File > Import Minecraft model JSON.
3. Import `mechanical_ore_washer_south.json` from
   `src/main/resources/assets/magneticraft2/models/multiblock/` in the mod repo.
   The importer anchors relative pixel-space geometry at the construction
   controller; it handles the model's unusually large/negative coordinates.
4. Use the Outliner search to locate and rename groups and inspect large numbers
   of elements. Move a beam with the gizmo; verify snapping and undo/redo.
5. Import matching PNG textures from the mod and inspect face UVs.
6. Save/reopen the Studio project. Verify geometry, selection, references, and
   the edited UVs persist.
7. Load the rotor model as a **separate group**. Pivot and motion import/creation
   is a follow-up milestone, not silently assumed from a baked model.

## Known limitations (do not overstate)

- Minecraft JSON import supports **`elements`, face UV, aliases, and axis-angle
  element rotations**. Unresolved `parent`/custom loaders are explicitly rejected
  or warned. It does not yet support Minecraft's `rescale` exactly.
- Imported namespace texture references are not automatic PNG bytes. Load the
  PNGs separately. Vanilla references use the procedural preview if missing.
- Hierarchy is group/part organization, **not** an implemented parent-child
  transform rig. Multi-selection and paintbrush UV editing are not implemented.
- The Forge exporter currently exports construction JSON; formed-model runtime
  and tiled block cell models are future work.
- Screenshots from CI verify window/controls and basic event handling; real
  Arch Linux/KDE input testing remains the next required acceptance step.

## Post-rework priorities

- Real import mapping for the washer's moving model parts, with named pivots.
- Multi-select, true group transforms, keyboard transform entry and align tools.
- Texture atlas/library and paint workflow using actual Magneticraft2 assets.
- Safe, validated tiled model export to the Forge mod (separate integration PR).
