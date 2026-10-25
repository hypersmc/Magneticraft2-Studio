# UV workspace: six faces and one editable atlas

The previous editor showed only the face currently chosen in a combo box. The UV workspace now exposes **all six sides of a selected cuboid at once**.

## Layout

- **Left/center: six-face overview** — an unfolded reference layout with Up above North, Down below North, West to its left, and East and South to its right. All six previews use their **own** texture resource and current UV coordinates. Click any tile (or use left/right arrows after focusing it) to choose the active face.
- **Below the overview: texture atlas** — a large nearest-neighbor view of the selected face's PNG. The selected rectangle is solid amber and has draggable corners. Other faces that happen to reference the **same texture resource** appear as dashed colored outlines. They are guides only and cannot accidentally modify another face.
- **Right: face properties** — texture selection, Auto UV, rotate/flip and exact UV coordinates for the currently selected face. They remain editable per face.
- **Beside the UV panel: live 3D preview** — inspect the assembled cuboid after each edit. Model gizmos are disabled while editing UVs, avoiding competing controls.

## What the overview means

The unfolded layout is a **non-destructive preview** of six *independent face mappings*, not an automatic atlas-packing operation. Importing a Java block model preserves its separate per-face texture IDs, UV rectangles and 90-degree rotations. The overview repeats the referenced PNG in tile units when a face spans multiple Minecraft blocks. It visibly shows a magenta checker when a PNG is missing rather than substituting an invented wood texture.

For a genuine unified Box-UV workflow (automatic creation, packing and resizing of connected UV islands), a future opt-in conversion tool is necessary. Do not silently rearrange existing face data.

## Regression checks

- With one selected cuboid, the six face previews are simultaneously visible and respond to clicks.
- Selecting Up in the net also selects Up in the drop-down, without modifying face UVs.
- Selecting a face with a different texture displays that texture in its own preview and in the editing atlas.
- Dragging the selected UV rectangle continues to write *only* that face's mapping, including Minecraft-pixel snapping and undo integration.
- Switching back to Model hides the UV panel and returns the full viewport.
- The initial Home/start screen, Java block bounds, multiblock layout and existing Studio project format remain unchanged.
