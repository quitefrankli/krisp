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

## Add a PBR material and lighting path

Replace the current Blinn-Phong path with a glTF metallic-roughness PBR path.
Use a Cook-Torrance microfacet BRDF with GGX/Trowbridge-Reitz normal
distribution, Smith masking-shadowing, and Fresnel-Schlick reflectance. Keep
the material evaluation and BRDF functions in shared shader code so static,
skinned, textured, and constant-colour variants do not duplicate the lighting
model.

Define one coherent PBR material that owns its scalar factors and references
its optional textures. The initial supported properties should be:

- base-colour factor and sRGB texture;
- metallic and roughness factors and the linear packed metallic-roughness
  texture;
- linear normal texture and normal scale;
- emissive factor and sRGB texture;
- linear occlusion texture and strength; and
- alpha mode, alpha cutoff, opacity, and double-sided state.

Import those properties according to glTF rather than translating them into
ambient, diffuse, specular, and shininess values. Treat
`KHR_materials_specular` as an optional extension to dielectric reflectance,
not as a legacy specular map. Update generated materials, presets, resource
provenance, scene serialization, fallback textures, and the material editor to
use the same PBR contract. No migration for old save data is required.

Give direct lights defined colour, intensity, and inverse-square distance
attenuation. Initially retaining the single active point light is acceptable.
Its direct contribution should combine energy-conserving Lambertian diffuse
with Cook-Torrance specular, then apply the existing visibility/shadow term.
Correct normal and tangent transforms for non-uniform scaling before relying on
the sharper PBR highlights.

The renderer already performs lighting in linear high-dynamic-range colour,
resolves to a floating-point scene image, and applies manual exposure plus
ACES-inspired tone mapping before sRGB presentation. Screenshots and video are
captured from the tone-mapped output; particles and overlays are HDR scene
content, while the GUI is composed afterward in SDR. HDR display output remains
out of scope.

Add image-based lighting after the direct-light path is correct. Support an HDR
environment source, diffuse irradiance, a roughness-prefiltered specular
cubemap, and a BRDF integration lookup texture. The existing sRGB skybox may be
displayed by the same environment but is not itself sufficient lighting data.

Implement in this order:

1. Introduce the PBR material contract and import, serialization, and editor
   support for the chosen glTF subset.
2. Correct texture colour spaces, normal transforms, and point-light radiometry.
3. Add the shared Cook-Torrance GGX/Smith/Schlick direct-light evaluation.
4. Add HDR environment preprocessing and split-sum image-based lighting.
5. Validate with focused material/import tests and reference scenes covering
   dielectrics, metals, roughness extremes, normal maps, emissive materials, and
   alpha modes; profile the added texture samples and render passes and record
   material findings in `docs/PERFORMANCE.md`.

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
