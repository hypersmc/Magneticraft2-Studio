# Magneticraft2 Studio — Linux Alpha 0.1

A first **runnable, local-only** multiblock editor for Magneticraft2 / Forge 1.20.1. It is a proof-of-concept editor, **not** an integrated in-game renderer. The Linux release is the priority; Windows packaging is deliberately deferred.

## Requirements (Arch Linux / KDE)

- Python 3 (for a loopback-only static web server; usually available on Arch).
- Chromium or Google Chrome or Brave for its dedicated standalone app window. Without those, `xdg-open` opens your default browser.
- A modern browser supporting Canvas 2D and ES modules.
- **No node_modules, npm install, CDN, cloud account or online service is required.**

## Run

Extract this directory, then execute:

```bash
./run-linux.sh
```

A separate Studio window opens. Closing it stops the local web server. Alternatively run `./run-linux.sh --no-browser` and open the printed localhost address. Press Ctrl+C in the terminal to stop. The server only listens on `127.0.0.1`, not your LAN.

Optional KDE application launcher:

```bash
./install-linux-desktop.sh
```

Remove the launcher later with `rm ~/.local/share/applications/magneticraft2-studio.desktop`.

## What works in this alpha

1. **New / resize**: create a multiblock with dimensions up to 128×128×128 (sparse in-memory editor). The legacy exporter has a 150,000-cell limit to avoid accidental huge JSON downloads.
2. **Construction**: paint role-based block cells by Y layer, place controller, add custom role IDs, add border frames, edit machine settings and preview the structure in an orbiting 3D view.
3. **Import**: open existing Magneticraft2 JSON structure layouts and Studio documents, including supplied faithful samples from `hypersmc/magneticraft2-1.20.x` (`mechanical_ore_washer_north`, `mechanical_sifter_north`).
4. **Formed model**: create an arbitrary number of cuboids, edit 1/16-block coordinates, group, duplicate, show/hide, change Minecraft texture resource IDs and move parts in the top-down plan. Generate a wooden frame or create cuboids from the construction layout.
5. **Motion preview**: assign rotation or oscillation animations to cuboids, choose axis, pivot and RPM, play/pause preview. Motion is **not** wired into the mod yet.
6. **Save / reopen** an editable `.mcraftstudio.json`; browser also stores an automatic local recovery copy.
7. **Forge export**: a ZIP with four direction-specific structure JSONs, per-block-cell baked static model JSONs (0..16 element bounds), one controller fallback model per direction, and a manifest for a future cell renderer. Animated parts are exported as metadata only.

## Important limitations

- The current Magneticraft2 mod renders its formed static Ore Washer at the controller. **A new mod-side renderer is needed** to display tiled static models on every formed filler cell. The Studio exporter **does not** magically make a large multiblock render correctly in-game yet.
- The supplied sample construction JSONs contain no formed-model geometry. Studio cannot reconstruct the actual Ore Washer or Sifter appearance from that information alone. We still need to import/convert their existing baked JSON models or manually model parts.
- The model preview uses colored material shading, not real block texture atlases or UV editing yet. Imported texture paths are written to exports, but an actual texture painter/UV editor is a future milestone.
- The animation rig is a preview + metadata schema, not code generation for the current Forge block entity renderer.
- Collision shapes, slice lighting, boundary culling, animations, chunk loading, state restoration, JEI and book compatibility all require integration/testing in Minecraft.
- Modeled geometry outside construction bounds is partially clipped by the static exporter, with warnings in its manifest. All parts must have `from < to` on each axis.
- Live viewport paint works best via the 2D grid. Moving a cuboid uses the plan drag or property fields, not 3D transform gizmos yet.

## Editor controls

| Action | Control |
|---|---|
| Orbit | Drag in 3D viewport |
| Pan | Shift+drag |
| Zoom | Scroll wheel |
| Paint construction | Click/drag across bottom grid; click 3D cell |
| Change layer | Left layer slider |
| Select part | Click model in 3D or object hierarchy |
| Move part X/Z | Drag its rectangle in the bottom plan view |
| Fit camera | `F` |
| Modes | `B` construction, `M` formed, `A` motion |
| Add cuboid | `G` in formed/motion mode |
| Delete selected cuboid | `Delete` |
| Save | `Ctrl+S` |

## Package layout

- `src/core.mjs` — edit format, import, rotation, slicing/export compiler
- `src/viewport.mjs` — dependency-free isometric 3D editor preview
- `src/zip.mjs` — browser ZIP writer
- `src/app.mjs` — interaction, UI, files, autosave
- `src/style.css` — Linux-first dark application UI
- `samples/` — real repo construction-format examples
- `studio_server.py` — localhost-only server + Chromium app window
- `tests/` — compiler and integrity tests

This is a separate editor repository/project candidate; no changes have been made to the Magneticraft2 game repository.
