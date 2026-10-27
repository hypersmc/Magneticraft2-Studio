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

1. Choose one of the six Java-supported machines.
2. Import the existing machine visual manifest and its animation scene, or
   edit the JSON text directly. Studio keeps unknown fields when saving.
3. Validate the pair before saving. The source list shows only actual Java
   angle/integer inputs available to the selected machine.
4. Use the shaft angle, Running, Overloaded, Worn and wear-stage controls to
   inspect numeric sampled poses. These controls are **preview-only**. The
   actual in-game shaft angle and states always come from Java/Gear V2.
5. Save to the **existing** `.mcraftstudio.json` under a new optional
   `visualRuntime` object. Older projects retain the old format and older
   `motions`/per-part animation metadata.
6. Optionally export the manifest and scene as **two actual runtime JSON
   resources**, under their correct namespaced paths.

## Important boundaries

- **This is not a complete resource-pack ZIP export.** Component models, wear
  variants and PNG dependencies must be supplied and checked separately.
- The pose inspector evaluates component-local rotation/translation, wear
  variants, state conditions and slider-crank values. It is **not** an
  OpenGL viewport rig: full parent/mechanism transform composition, pulleys,
  belt mesh + UV travel and facing transforms are not yet drawn.
- The Java belt-loop renderer currently only opts in for the Bronze Stamp Mill;
  Studio warns on other machine IDs rather than claiming they will appear.
- The old `AnimationExport` rotation-only bridge remains available, unchanged.
- Existing multiblock construction and formed-static model export are
  intentionally separate, as the mod uses Rendering V2 split-cell meshes.

## Next milestones, incrementally

1. Promote the scene/component metadata into context-sensitive editing tools
   instead of requiring raw JSON. Integrate with Outliner.
2. Compose and render the scene hierarchy into the actual Qt/OpenGL viewport
   with the canonical SOUTH/controller reference frame and real baked model
   dependencies.
3. Evaluate and render typed belts using the precise Java `BeltPath` tangent,
   arc, clearance, tessellation and UV equations, and validate texture files.
4. Export **real** Minecraft model JSON, all block-atlas PNGs and a validated
   pack-format 15 ZIP, preserving existing V2 static meshes.
5. Test all six machines in a live Forge 1.20.1 client with F3+T and verify
   Java fallback behavior and frame-rate performance.

## Linux testing

```bash
cmake -S . -B build -DSTUDIO_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The `studio-visual-contract-tests` target checks manifest/scene validation,
physical sources, cycles, deterministic poses and non-destructive JSON
export/re-import against all six mod fixtures.
