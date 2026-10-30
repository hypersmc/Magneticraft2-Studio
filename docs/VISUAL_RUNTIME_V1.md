# Native Studio / Magneticraft2 visual runtime v1

This branch extends the original [PR #3](https://github.com/hypersmc/Magneticraft2-Studio/pull/3)
rather than replacing it or merging the deleted mod animation branch.

## Authoritative source

The mod's [PR #12](https://github.com/hypersmc/magneticraft2-1.20.x/pull/12)
was merged into `feature/bronze-age-foundry` on 2026-10-27, merge commit
`20384e31c6058261c9ed9ba156b239f3347c5c5b`.
Do not use the deleted source branch as a live reference.

The two resource schemas are **not** new Studio inventions:

- `assets/magneticraft2/machine_visuals/<java_machine>.json`
- `assets/<namespace>/animations/<scene_path>.json`

They are tested against the *six real machine manifest + scene pairs* captured
from the merged mod branch under `tests/fixtures/animation-v1/`.

## Implemented first-stage workflow

In a multiblock project, open **File → Edit Machine Visual JSON (runtime v1)...**

1. Choose one of the six Java-supported machines. **Studio automatically
   loads the matching, tested v1 Java manifest AND animation scene together.**
   These examples are compiled into the native executable; no mod source is
   required. Changing machines never reuses the Grinder's default input.
2. If the mod is installed, click **Open Magneticraft2 JAR...** and select
   the installed `.jar` (or distributed `.zip`). This imports the **actual**
   manifest, scene, model JSON and PNG resources from that mod version.
   Alternatively, edit the bundled JSON text directly, or import your own
   JSON definitions individually. Your custom/unknown fields survive save,
   and switching machines prompts before replacing unsaved edits.
3. Validate the pair before saving. The source list shows only actual Java
   angle/integer inputs available to the selected machine.
4. Select an actual Java shaft channel and scrub its physical angle. Running,
   Overloaded, Worn and wear-stage controls update both the sampled pose and
   the OpenGL machine preview. Play/Pause runs an explicitly preview-only
   signed degrees/second rate (never exported as mechanical speed).
5. The **Open Magneticraft2 JAR...** workflow loads real scene geometry
   without a checkout. For modders with unpacked resource packs, the
   **Load model assets from folder...** option also accepts a directory
   containing `assets/`. Neither path modifies the installed JAR or uses
   an external unzip command. Referenced models are cached as actual
   cuboid geometry; missing models are explicitly reported. If vanilla
   Minecraft textures are absent, select the local client JAR through
   **Add missing PNGs from Minecraft JAR...**.
6. Save to the **existing** `.mcraftstudio.json` under a new optional
   `visualRuntime` object. Older projects retain the old format and older
   `motions`/per-part animation metadata.
7. Optionally export the manifest and scene as **two actual runtime JSON
   resources**, under their correct namespaced paths.

## Live native OpenGL preview (implemented)

- Rig components use the existing Studio mesh shader, UV handling and texture cache. Each distinct model is built and uploaded once per asset import; animation updates GPU matrices only.
- Hierarchy, pivots, static and driven rotations, translations and visibility follow the Java canonical SOUTH/controller-relative transform order.
- Sifter crank, rigid connecting rod, follower and slider use the actual Java X/Z slider-crank equations, not independent sine approximations.
- State-specific tracks, wear-stage model variants and independent real Java shaft inputs can be inspected and scrubbed.
- Stamp Mill open belts use Java-style outer tangents and arc subdivisions. Physical UV repeats and signed cumulative shaft-angle surface movement are handled in the GPU shader, not stretched across the entire belt.
- Models can be loaded from a local distributed mod JAR/ZIP or an unpacked assets directory; parent-only models and texture-slot inheritance are resolved. Missing models are reported, missing PNGs are displayed with obvious magenta checkers.

## Important boundaries

- Export still writes only the two valid runtime JSON resources. It does not yet create a full pack-format-15 resource ZIP or generate missing baked Minecraft models and PNG dependencies.
- The GPU scene preview uses canonical SOUTH orientation; accurate N/E/W machine-facing transforms and complete Rendering V2 static model assembly are still future tasks.
- The editor offers direct JSON editing, not yet a full typed animation Outliner or mechanism gizmo interface.
- The belt renderer is opted in only for the Bronze Stamp Mill in Java. Unsupported machines are warned, not silently granted belt rendering.
- Vanilla Minecraft model features, lighting, transparency sorting, culling and in-game shader parity need visual comparisons. For vanilla textures use your installed client JAR; no Mojang textures are bundled or downloaded by Studio.
- The old Studio animation exporter and existing .mcraftstudio.json project format remain supported. Preview degrees/second are not real Gear V2 RPM and are never exported.

## Next milestones

1. Add visual rig hierarchy, inputs, tracks and mechanism inspectors, with direct joint manipulation.
2. Add whole-machine N/E/S/W preview, source model feature coverage and screenshot comparison with Forge 1.20.1.
3. Build and validate a full resource-pack ZIP with real authored model JSON, correct atlas PNGs and direct belt textures.
4. Perform live Minecraft F3+T testing, including machine fallback, all six machine scenes, and performance with multiple machines.
## Linux testing

```bash
cmake -S . -B build -DSTUDIO_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The `studio-visual-contract-tests` and `studio-rig-preview-tests` check six actual
Java scene fixtures, slider-crank rod closure, inherited parent transforms,
wear variants, belt UV tiling, and unwrapped shaft-angle travel. Linux CI also
runs a native Qt/OpenGL window test that imports real Grinder model geometry.
Studio's bundled templates are Java runtime v1 snapshots. When the game mod
changes, prefer loading the runtime pair from the installed mod JAR to avoid
editing definitions from a different version.

Tests also construct a small local ZIP/JAR, proving that all six Java
manifest/scene pairs and actual grinder baked models can be loaded without
a mod source checkout or external archive extraction.

These are not substitutes for live Forge 1.20.1 in-game visual verification.
