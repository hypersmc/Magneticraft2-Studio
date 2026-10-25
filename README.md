# Magneticraft2 Studio — Native Linux editor v0.5 (design iteration)

This is a **draft native editor**, not a finished Blockbench replacement. v0.5
redesigns the workspaces around the modeling workflow. See
[editor experience and Ore Washer acceptance exercise](docs/EDITOR_UX.md).

### Running on Arch Linux

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-wayland
./run-linux.sh
```

### Workspaces

- **BUILD:** construction layers, roles and controller placement.
- **MODEL:** full-screen priority 3D viewport, tool rail, searchable Outliner,
  selection inspector, material previews and numeric transforms.
- **UV / TEXTURE:** the same model document with face-level UV editor and PNG import.

File > Import Minecraft model JSON supports native Minecraft Java block-model
`elements`, `from`/`to`, face textures/UVs and element axis rotations. It is
specifically intended to help us bring in the real Ore Washer/Sifter geometry
from Magneticraft2's resources; see the acceptance exercise for limitations.

Use **F** to frame the model, **Shift+F** to frame the selected element,
**V/B/W/E/R** for select/draw/move/rotate/resize and **Ctrl+I** to import a
Minecraft model. Right-click any item in the Outliner to rename, hide, duplicate
or frame it. Import PNG textures in the UV workspace.

---

# Magneticraft2 Studio (Native Linux Alpha)

A **native Qt 6 / C++17 / OpenGL 3.3** multiblock editor for Magneticraft2 (Minecraft Forge 1.20.1). This replaces the web-wrapper approach as the proposed Linux-first foundation. Windows support is deferred until the Linux experience is satisfactory.

The `linux-alpha` branch preserves the original web prototype. The `native-linux` branch contains this independent native implementation. **Not a finished game renderer or full Blockbench replacement.**

## Dedicated Model workspace (v0.5)

Choose **WORKSPACE → Model** to author the actual *formed machine*, independently
of the construction blocks. The layer editor collapses so the GPU viewport
fills the central workspace. Objects are grouped in the Outliner at left and
can be edited precisely in the Properties dock at right.

- **V · Select**: click an existing cuboid without accidentally moving it.
- **B · Draw cube**: choose **PLANE Y** (workplane elevation), **HEIGHT**
  (extrusion), and **SNAP**; then **click-drag directly in the 3D viewport**.
  A dashed preview displays the new shape. Dragging backwards is supported.
- **W / E / R**: Move, Rotate or Resize the selected cube. Drag its visible
  red/green/blue XYZ handles, plane squares or center handle.
- **X / Y / Z**: change the active transform axis.
- **VIEW**: Perspective, Top, Front, Right. Top/Front/Right are orthographic.
- **SNAP**: 1/16 block (one Minecraft model pixel), 1/8, 1/4, 1/2, or 1 block.
- **Outliner**: named groups, rename/duplicate/delete geometry, material and
  visibility controls, and undo/redo.
- **Textures / UV**: import PNGs, preview them on the actual GPU model,
  edit individual face UVs and repeat pixel art without texture stretching.

Try drawing a beam across several blocks: select **Model**, choose **B**,
set **VIEW = Top**, **PLANE Y = 1**, **HEIGHT = 1/4**, **SNAP = 1/16**,
and drag in the viewport. Then select the beam and resize it with the handles.
Save and reopen the `.mcraftstudio.json` project to verify it persists.

## Arch Linux / KDE: build and run

Install build dependencies:

```bash
sudo pacman -S --needed base-devel cmake qt6-base
```

Then, from this repository:

```bash
./run-linux.sh
```

This creates a local `build/` directory and launches a **real desktop binary**. No Python server, Electron, Chromium, CDN, account or internet connection is used at runtime. If running in a Wayland session and the Qt platform plugin is unavailable, install the Qt Wayland platform support provided by your distribution.

Manual CMake alternative:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
./build/magneticraft2-studio
```

Run native tests:

```bash
ctest --test-dir build --output-on-failure
```

## Implemented in this milestone

- A real Linux window using **Qt dockable panels**, native menus/dialogs, dark application theme and saved window layout.
- GPU-accelerated OpenGL 3.3 viewport with perspective camera, orbit/pan/zoom, selection, a grid and draggable RGB/plane/center transform gizmos.
- Construction layer editor: paint with registered block-role identifiers and resize dimensions from 1 to 128 blocks on each axis. Native model dimensions are *not* limited to 3 x 3 x 3.
- Dedicated formed **Model** workspace with direct click-and-drag cuboid drawing, live preview, editable workplane/height, orthographic camera views, grouped outliner and **draggable XYZ axis handles**. No 3×3×3 editing limitation.
- 3D geometry assembly runs via a **single background worker thread** and returns completed meshes to the GUI without issuing OpenGL commands outside the GUI thread. Face culling is applied to enclosed construction block faces.
- Save and reopen the previous web alpha's `.mcraftstudio.json` **schema v1**. Unknown top-level project and part fields are retained on save. Imported animation data is preserved, but animations are not simulated by this native alpha.
- Import existing Magneticraft2 `structure.layout.layerN` construction files (sample Washer/Sifter files provided). North/east/south/west imported layouts are mapped to the canonical local orientation.
- Export the four rotated JSON construction layouts plus a manifest in a selected directory. **Formed-model export is not implemented yet.** This is deliberate: writing incomplete formed models would be misleading or break the mod.
- In-memory undo/redo, unsaved change confirmation, atomic file saving via `QSaveFile` and keyboard shortcuts.
- Headless C++ geometry/UV tests without Qt, Qt-backed PNG/UV save/load tests, legacy-import tests and Xvfb/Qt mouse tests for direct cube drawing, gizmos and UV dragging.

## How to use

| Action | Input |
|---|---|
| Orbit | Left-drag empty viewport |
| Pan | Middle/right-drag |
| Zoom | Mouse wheel |
| Paint construction | Click/drag in the bottom layer canvas, select role in BRUSH dropdown |
| Change Y layer | Layer number next to the bottom canvas |
| Draw geometry | Model workspace → B, set workplane and height, drag over grid |
| New model part | `Edit -> Add cuboid` or `+ Box` toolbar |
| Select a part | Choose V, then click a cuboid or select it in the Outliner |
| Move a selected part | W: drag RGB axis arrows, plane squares or white center handle |
| Rotate a selected part | E: drag the matching colored rotation ring |
| Resize a selected part | R: drag axis square or white center for uniform resize |
| Switch tool/axis | V B W E R / X Y Z while viewport has focus |
| Change exact coordinates | Right Properties inspector |
| Import / apply textures | Right Textures / UV tab or Textures menu |
| Per-face UV editing | Choose face in UV tab, drag rectangular handles or type UV tile coordinates |
| Fit view | `F` or toolbar Fit |
| Undo / redo | Ctrl+Z / Ctrl+Shift+Z |
| Save project | Ctrl+S |
| Open project/legacy JSON | Ctrl+O |

## Architecture

```text
src/core/Project.*    Qt-independent document schema, cell operations, rotation, slicing
src/core/Geometry.*   Qt-independent mesh generation, adjacency face culling
src/core/Modeling.*   Direct-draw cuboids and snap validation
src/io/ProjectIO.*    Qt JSON import, legacy import, atomic save, construction export
src/ui/Viewport.*     QOpenGLWidget + GL shaders/VBO, background CPU mesh worker
src/ui/LayerCanvas.*  Native layered block painting widget
src/ui/UvEditor.*     Native PNG preview and draggable per-face UV coordinates
src/ui/MainWindow.*   Qt dockable editor, tool actions, inspector, undo/redo
src/main.cpp          OpenGL context and native Qt desktop theme
samples/              Existing Magneticraft2 Ore Washer/Sifter construction layouts
tests/                Portable C++ core and Qt JSON IO regression tests
```

The UI and Forge exporter are separate from the project/geometry layer. A later native asset pipeline can reuse or replace the model slicing functions without modifying the editor document schema.

## Deliberately not claimed as complete

- Pixel brush painting, imported baked Java models or a `.bbmodel` importer.
- Fully editable animation rigs, IK, gear linkage constraints or physics/simulation.
- Full Blockbench-equivalent vertex/face modeling, freeform meshes or animation rigs.
- Formed-model tiled Minecraft block assets, animated BER metadata or mod-side runtime integration.
- AppImage/pacman releases, CI packaging, Windows, macOS.
- GUI testing on the target Arch/KDE desktop. Linux CI provides a Qt/OpenGL startup smoke test; interactive mouse-driven modeling still needs manual validation.

**Do not merge until the native editor builds on Arch, opens existing Studio files, and passes a real visual check.**

## Engineering notes

- Data uses real block coordinates, while editing snaps to 1/16 of a block. Direction transforms apply to the *entire structure*, not individual arbitrary cube rotation.
- Export safety limit: 150,000 construction cells for dense legacy JSON, or up to 128 blocks/axis for editor layout.
- Rotated cuboids and animated parts are not flattened by the static slicing helper; the helper reports them as unsupported until the renderer/asset compiler can preserve semantics.
- The 3D viewport now renders nearest-neighbor PNG pixels with per-face UVs. Forge-side rendering still needs UV, lighting, occlusion and chunk-boundary integration.
- Construction-only exports do not automatically yield a working formed multiblock in Forge. The mod's filler cell renderer will require a separate PR.


## v0.5 native editing: gizmos and PNG / UV mapping

The **Model** workspace has interactive RGB move/rotate/resize handles.
- W **Move**: drag red/green/blue axis arrows; drag a tinted plane square for XY/XZ/YZ; drag the **white center** to move freely in a camera-facing plane.
- E **Rotate**: drag the colored X/Y/Z rings, 5-degree snapping.
- R **Resize**: drag the colored square handles for one dimension, or the **white center** for all dimensions; snapped to your selected grid.
- Mouse clicks on a cuboid select it. Geometry only changes while a gizmo is dragged.

The new **TEXTURES / UV** dock (right-side tab) lets you import PNG images (up to 4096x4096, 6 MiB), preview them directly on the GPU model, choose a cube face, override its PNG, and drag/resize its UV rectangle over a pixel-aligned image. You can also type UV coordinates, rotate UVs in 90-degree steps, flip U/V, or restore Auto UV. New cuboids default to **one texture tile per Minecraft block**, avoiding stretched pixels across beams.

Imported PNG files are embedded in your `.mcraftstudio.json` project, so files remain portable and offline. Existing Studio v1 projects and legacy structures still load. External Minecraft texture IDs remain metadata only until you import the corresponding image; the viewport uses generated material-specific pixel textures as placeholders rather than pretending to have Minecraft's atlas.

**Deliberately still out of scope:** face picking via 3D raycasts for direct painting, an image brush/paint layer, Blockbench `.bbmodel` importer, texture-atlas packing for Minecraft export, complex group hierarchies, animation constraints, and Forge tiled formed-model rendering. These remain later milestones.
