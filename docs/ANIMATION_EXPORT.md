# Magneticraft2 animation export — native Linux Studio bridge

The `native-linux` Studio branch now has an **export-only** integration with
Magneticraft2 Forge 1.20.1's JSON-driven animation runtime. The Java mod's
development branch is `feature/json-driven-animation` (draft PR #12).

This is **not** a finished animation timeline, real-time motion simulator,
model generator, or automatic binding of arbitrary machines. Studio continues
to preserve unknown project metadata, and the export-only bridge validates
explicit bindings authored in the saved `.mcraftstudio.json` project.

## How to try it

1. Create or open a **multiblock** project. Save it.
2. For each animated Part, add an `animation` object to that Part's saved
   JSON, and assign a `motionId`. Only these explicitly bound Parts become
   animated components. Other Parts remain normal Studio geometry.
3. Add the corresponding entry to the top-level `motions` array. Save, then
   **reopen the edited project** in Studio.
4. Use **File → Export Magneticraft2 animation JSON…** and save to e.g.
   `assets/magneticraft2/animations/bronze_mechanical_lathe.json`.
5. Supply *real baked Minecraft model files* referenced by your bindings,
   such as `assets/magneticraft2/models/animated/input_rotor.json`.
6. In Minecraft, activate the resource pack or install the assets and use
   **F3+T**. The Java machine must already opt in to the animation ID.

Example fragments in a Studio project, with two Parts under `parts`:

```json
{
  "parts": [
    {
      "id": "input_rotor",
      "name": "Gearbox shaft",
      "motionId": "drive",
      "animation": {
        "model": "magneticraft2:animated/input_rotor",
        "model_offset": [0, 0, -2],
        "pivot": [0.5, 0.5, 0.5],
        "render_layer": "cutout"
      }
    },
    {
      "id": "marker",
      "name": "Wear mark",
      "animation": {
        "model": "magneticraft2:animated/marker",
        "parent": "input_rotor",
        "model_offset": [0, 0, 0],
        "pivot": [0, 0, 0],
        "visible_when": "running"
      }
    }
  ],
  "motions": [
    {
      "id": "drive",
      "type": "rotate",
      "axis": "z",
      "driver_source": "mechanical.visual_angle_degrees",
      "driver_module": "mechanical_input",
      "scale": 1.0,
      "when": "always"
    }
  ]
}
```

This fragment shows the relevant keys only; retain the geometry/texture fields
that Studio writes for each Part, plus the project's dimensions and palette.
The `marker` inherits the input rotor's rotation without an independent track.

## Coordinate and driver contract

The **exported animation JSON** uses:
- `format: magneticraft2:visual_animation`, `format_version: 1`.
- **Block units**, controller origin, and **canonical SOUTH** facing.
- `model_offset` relative to the parent component, or the controller if
  the component has no parent.
- `pivot` in the component model's local block coordinates.
- Signed axes `x/y/z/-x/-y/-z` and finite `scale` (maximum magnitude 128).
- Named Gear V2 module inputs. `mechanical_input` is available on the current
  Bronze Lathe renderer; `spindle` is also exposed to JSON by that renderer.
- Conditions `always`, `running`, `stopped`, `overloaded`, and `worn`.
  Java supplies the actual state; JSON cannot invent the mechanical state.
- Optional transform-only parenting nodes omit `model` (but must still have
  explicit `model_offset` and `pivot`).

**Important:** Native Studio's construction grid is historically NORTH
oriented. The animation metadata above is **explicitly SOUTH/controller-relative**
and is exported exactly as authored. Studio does not currently automatically
convert drawing coordinates or infer pivots from scene geometry. Do not
assume an existing Part's global bounds can be pasted into these fields.

`Motion.speed` was the old editor's conceptual preview speed. It is **not**
exported as mechanical RPM or Gear V2 gearing. Instead, the explicit `scale`
is a signed visual ratio of the actual Gear V2 shaft angle. A missing scale
defaults to 1.0. The `when` field controls when a driven rotation track
applies, while `visible_when` controls component visibility.

## Validation and limits

The exporter rejects missing model references for a driven Part, unknown
motion IDs, unsupported drivers/axes, malformed coordinates, duplicate IDs,
invalid/recursive parents and extreme multipliers. It writes atomically and
does not change server multiblock, V2 static models, voxel collision, or recipes.

It does **not** create Minecraft model geometry, animated UVs, fluid streams,
complex crank-linkage constraints or arbitrary state/keyframe timelines. Those
are future capabilities; do not merge the Studio bridge as a complete visual
animation editor.

Run the Qt IO test suite where Qt6 is available:

```bash
cmake -S . -B build -DSTUDIO_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A test covers two parts sharing a motion, an inherited parent, a running-only
child, legacy speed not being confused with scale, and invalid parent rejection.

The Java mod separately requires `./gradlew build` and in-game verification
for its animation branch. Native Studio code and Java mod code live in
**separate repositories and feature branches**.
