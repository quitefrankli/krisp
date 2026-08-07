# Architecture TODOs

## Encapsulate mutable skeletal poses

Animation updates and render-frame pose snapshots are game-thread confined,
and graphics consumes only copied bone transforms. Bone definitions are now
private and `get_bones()` returns a const reference. However,
`get_bone_local_transform()` still returns a mutable transform reference, so
pose mutation is not expressed through an explicit skeletal-component operation.

Replace direct mutable access with explicit pose operations. This should
include animation updates, IK, editor manipulation, deserialization, and
tests. Once all callers use those operations, remove the remaining mutable pose
reference. Add synchronization only if pose mutation is later moved off the game
thread.

## Add image-based PBR lighting

Stage 2 adds base-colour, metallic-roughness, and tangent-space normal textures,
but deliberately retains Stage 1's direct point-light illumination. Add
environment lighting as a separate stage: define environment-map resources and
preprocessing, diffuse irradiance, roughness-prefiltered specular lighting, and
a BRDF integration lookup texture. Do not approximate indirect illumination
with a constant ambient term while this work remains outstanding.

## Support compressed textures in glTF

Standalone texture resources support DXT5/BC3 DDS, but core glTF image sources
remain PNG and JPEG. Add `MSFT_texture_dds` only as an explicit glTF extension:
validate its fallback and extension declarations, preserve the existing texture
semantic/colour-space rules, and test external URIs, data URIs, and GLB buffer
views. Do not infer DDS support from image bytes in core glTF materials.

Emissive output, occlusion, alpha masking/blending, double-sided rendering,
additional texture-coordinate sets, and other material or texture extensions
remain separate scope choices, not implied requirements of either item above.

## Replace the coarse GUI mutex with asynchronous state exchange

The current manager-wide mutex is a temporary correctness boundary between
game-thread `process()` calls and graphics-thread `draw()` calls. It prevents
data races, but serializes the two loops and allows scene saves, resource loads,
and other expensive panel actions to stall GUI rendering.

Replace shared mutable window state with explicit thread ownership:

- The graphics thread exclusively owns ImGui, window visibility, selections,
  text buffers, and other widget state.
- The game thread exclusively owns engine state and executes every operation
  that mutates the scene, ECS, audio, or game/application state.
- Send discrete UI actions from graphics to game through a bounded SPSC command
  queue. Coalesce replaceable values such as sliders so stale intermediate
  updates cannot fill the queue; define explicit overflow handling for actions
  that must not be dropped.
- Publish game-to-GUI data as immutable, latest-wins snapshots using a
  double/triple-buffered mailbox with atomic slot publication. Snapshots should
  contain only stable IDs and copied display data, never references into mutable
  engine containers.
- Keep file and resource work outside synchronization sections. Return its
  result, refreshed lists, and error status in a later snapshot.
- Apply the same model to engine panels, persistent overlays, application UI,
  and editor keyboard commands rather than retaining special cross-thread paths.

Migrate one panel at a time behind shared command/snapshot infrastructure. Once
all GUI state has a single owner, remove `EngineUiManager::state_mutex`. Add
unit tests for command ordering, coalescing, capacity/overflow policy, and
snapshot consistency; run a ThreadSanitizer stress test and record frame/tick
timings before and after removing the mutex.

## Expand Jolt integration

- Add a native Jolt debug renderer for collision shapes and remove dependence
  on the retired engine collider visualizer.
- Add engine-owned APIs for constraints, vehicles, ragdolls, and soft bodies
  when an application needs them.
- Add static triangle-mesh shapes and Jolt `CharacterVirtual` locomotion.
