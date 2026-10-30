# Animate workspace — shared Studio document

This is the native Animate tab in Magneticraft2 Studio (Qt 6/OpenGL),
not a separate machine-modeling program or a replacement for Blockbench.

## Work in one machine document

1. **Build** — define how players assemble the multiblock.
2. **Model** — create and edit cuboids in the formed machine model.
3. **UV / Texture** — map each Minecraft cuboid face and import custom PNGs.
4. **Animate** — choose a rig component and drive it from the actual
   Magneticraft2 runtime-v1 physical inputs, while seeing the same static
   model in the same OpenGL viewport.

Normal Java block models deliberately do not expose the mod-only Animate tab:
normal Java block JSON has no supported Forge visual machine runtime.

## First animation of an authored part

- Open/create a multiblock project with a cuboid, then choose ANIMATE.
- Choose the Java machine; the six bundled machine definitions each have
  their own exact v1 manifest and animation scene.
- Select a model cuboid in the normal Outliner, choose a rig Component
  in Animate, then click "Bind selected cuboid to joint".
- Its binding is saved as a new optional "rigComponent" string on the
  project part. This never modifies the existing "motionId" used by the
  legacy animation bridge.
- If the chosen Java rig pivot is not appropriate for your new geometry,
  click "Use selected cuboid's pivot" (an explicit action) or edit X/Y/Z.
  The authored pivot is not changed behind the user's back.
- Select an existing rotation track or "New rotation", choose the scene
  input, axis, ratio, phase and condition, and click "Add / update rotation".
  Changes are validated against the existing Java runtime schema before
  being saved into the optional "visualRuntime" project metadata.
- Set a physical Java shaft angle or click Play. Preview speed is editor-only:
  actual in-game RPM always comes from the mod mechanical simulation.

While Animate is visible, bound authored cuboids leave the static model mesh
and are rendered through their assigned rig instead. They return to ordinary
editable parts in Model/UV. The animated geometry is built once after an edit,
transformed into the rig's zero-angle reference frame, and uploaded to the
GPU; each preview frame changes matrices and belt texture phase, not mesh
buffers. Multiple cuboids may bind to the same rig component.

## Load assets once, not on every machine

- "Choose mod JAR..." selects the installed Magneticraft2 Forge mod JAR.
  Studio reads actual model/texture resources directly; a source checkout is
  not required. If JAR definitions differ, Studio asks before replacing
  authored JSON with the mod version's manifest and scene.
- "Assets folder..." supports unpacked packs or a checkout when available.
- "Minecraft textures..." remembers the client JAR / ZIP for vanilla PNGs.
- The mod source path is saved in local app settings, optionally recorded as
  a project-local reference under visualRuntime.assetSource. On reopening
  or entering Animate, valid sources restore automatically. A bounded
  three-machine CPU cache prevents repeated JAR decoding on each tab switch.
- "Reload assets" explicitly clears that source cache after an update.
  Mod JAR file timestamp and size also form part of the cache key.
- Neither mod nor Minecraft copyrighted texture PNGs are bundled with Studio.
  Authored project PNGs are separate and are never silently overwritten by
  an imported rig texture.

Projects moved between computers retain authored model parts, embedded custom
textures and animation definitions. They may need a new local asset path
when the original JAR is unavailable.

## Runtime contract and remaining work

The editor uses the exact Forge-v1 machine_visuals and visual_animation JSON
formats, with real per-machine Java channel names. It is not an unrelated,
generic fixed-speed/keyframe animation engine.

Current preview covers rotations, sine/cosine translation, parent/child
hierarchy, pivots, state conditions, wear variants, the Sifter's rigid
slider-crank and the Java-opted-in Stamp Mill belt. Authored static and moving
model parts share one viewport.

Not implemented/verified yet:

- Dragging rig pivots directly with a 3D gizmo (numeric and selected-cuboid
  pivot editing are available).
- Fully visual rig graph and constraint Outliner.
- Complete validated Minecraft model/texture/animation ZIP export.
- Complete Rendering V2 formed body, fluids, items, particles, lighting,
  transparency sorting and N/E/W machine orientation.
- Side-by-side live Forge 1.20.1 acceptance and performance tests.

Legacy Studio motions and original rotation-export bridge remain supported.
The advanced two-JSON dialog remains under File for raw JSON editing, but the
main Animate tab is the intended design-to-texture-to-animation workflow.
