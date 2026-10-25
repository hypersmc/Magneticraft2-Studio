# Magneticraft2 Studio (Native Linux Alpha)

A **native Qt 6 / C++17 / OpenGL 3.3** multiblock editor for Magneticraft2 (Minecraft Forge 1.20.1). This replaces the web-wrapper approach as the proposed Linux-first foundation. Windows support is deferred until the Linux experience is satisfactory.

The `linux-alpha` branch preserves the original web prototype. The `native-linux` branch contains this independent native implementation. **Not a finished game renderer or full Blockbench replacement.**

## Dedicated Model workspace (v0.3)

Choose **WORKSPACE → Model** to author the actual *formed machine*, independently
of the construction blocks. The layer editor collapses so the GPU viewport
fills the central workspace. Objects are grouped in the Outliner at left and
can be edited precisely in the Properties dock at right.

- **V · Select**: click an existing cuboid without accidentally moving it.
- **B · Draw cube**: choose **PLANE Y** (workplane elevation), **HEIGHT**
  (extrusion), and **SNAP**; then **click-drag directly in the 3D viewport**.
  A dashed preview displays the new shape. Dragging backwards is supported.
- **W / E / R**: Move, Rotate or Resize the selected cube. Drag its visible
  red/green/blue XYZ handles or the object body along the active axis.
- **X / Y / Z**: change the active transform axis.
- **VIEW**: Perspective, Top, Front, Right. Top/Front/Right are orthographic.
- **SNAP**: 1/16 block (one Minecraft model pixel), 1/8, 1/4, 1/2, or 1 block.
- **Outliner**: named groups, rename/duplicate/delete geometry, material and
  visibility controls, and undo/redo.
- **Texture ID**: a part's resource path can be recorded in project JSON,
  but **texture display and UV painting are not yet implemented**.

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
- GPU-accelerated OpenGL 3.3 viewport with perspective camera, orbit/pan/zoom, selection, a grid and selected-object axis guides.
- Construction layer editor: paint with registered block-role identifiers and resize dimensions from 1 to 128 blocks on each axis. Native model dimensions are *not* limited to 3 x 3 x 3.
- Dedicated formed **Model** workspace with direct click-and-drag cuboid drawing, live preview, editable workplane/height, orthographic camera views, grouped outliner and **draggable XYZ axis handles**. No 3×3×3 editing limitation.
- 3D geometry assembly runs via a **single background worker thread** and returns completed meshes to the GUI without issuing OpenGL commands outside the GUI thread. Face culling is applied to enclosed construction block faces.
- Save and reopen the previous web alpha's `.mcraftstudio.json` **schema v1**. Unknown top-level project and part fields are retained on save. Imported animation data is preserved, but animations are not simulated by this native alpha.
- Import existing Magneticraft2 `structure.layout.layerN` construction files (sample Washer/Sifter files provided). North/east/south/west imported layouts are mapped to the canonical local orientation.
- Export the four rotated JSON construction layouts plus a manifest in a selected directory. **Formed-model export is not implemented yet.** This is deliberate: writing incomplete formed models would be misleading or break the mod.
- In-memory undo/redo, unsaved change confirmation, atomic file saving via `QSaveFile` and keyboard shortcuts.
- Headless C++ geometry tests that can run without Qt, plus Qt-backed JSON round-trip, legacy-import tests and an Xvfb/Qt mouse-drag test that draws a cube in the OpenGL viewport.

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
| Move a selected part | Set tool W and an axis, drag selected part in viewport |
| Rotate a selected part | Set tool E and axis, drag selected part |
| Scale a selected part | Set tool R and axis, drag selected part |
| Switch tool/axis | V B W E R / X Y Z while viewport has focus |
| Change exact coordinates | Right Properties inspector |
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
src/ui/MainWindow.*   Qt dockable editor, tool actions, inspector, undo/redo
src/main.cpp          OpenGL context and native Qt desktop theme
samples/              Existing Magneticraft2 Ore Washer/Sifter construction layouts
tests/                Portable C++ core and Qt JSON IO regression tests
```

The UI and Forge exporter are separate from the project/geometry layer. A later native asset pipeline can reuse or replace the model slicing functions without modifying the editor document schema.

## Deliberately not claimed as complete

- Texture painting, UV editing, imported baked Java models or a `.bbmodel` importer.
- Fully editable animation rigs, IK, gear linkage constraints or physics/simulation.
- Full Blockbench-equivalent vertex/face modeling, freeform meshes, UV editing and texture previews.
- Formed-model tiled Minecraft block assets, animated BER metadata or mod-side runtime integration.
- AppImage/pacman releases, CI packaging, Windows, macOS.
- GUI testing on the target Arch/KDE desktop. Linux CI provides a Qt/OpenGL startup smoke test; interactive mouse-driven modeling still needs manual validation.

**Do not merge until the native editor builds on Arch, opens existing Studio files, and passes a real visual check.**

## Engineering notes

- Data uses real block coordinates, while editing snaps to 1/16 of a block. Direction transforms apply to the *entire structure*, not individual arbitrary cube rotation.
- Export safety limit: 150,000 construction cells for dense legacy JSON, or up to 128 blocks/axis for editor layout.
- Rotated cuboids and animated parts are not flattened by the static slicing helper; the helper reports them as unsupported until the renderer/asset compiler can preserve semantics.
- The 3D preview renders colored materials rather than textures. Any future renderer must preserve UVs, lighting, occlusion and chunk boundaries.
- Construction-only exports do not automatically yield a working formed multiblock in Forge. The mod's filler cell renderer will require a separate PR.
