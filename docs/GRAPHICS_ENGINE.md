# Graphics Engine Architecture

`GraphicsEngine` is Krisp's Vulkan-backed rendering boundary. The game layer
owns mutable scene state and publishes completed immutable snapshots; the
graphics engine owns the window-facing Vulkan objects, GPU representations,
and render loop. Ray tracing is unsupported and its C++ and shader build paths
remain disabled.

## Major components

| Area | Responsibility |
| --- | --- |
| `graphics_engine.*` | Top-level coordinator: owns the render loop, frame mailbox, graphics renderables, and submodules. |
| `graphics_engine_instance/device/swap_chain.*` | Creates the Vulkan instance, surface, device, queues, swap chain, and per-swap-chain frames. |
| `graphics_engine_frame.*` | Records and submits one frame's command buffer; owns its fences, semaphores, and transient per-frame resources. |
| `render_draw_list.*` | Caches renderable-level pass classification and state ordering for reconciled topology. |
| `resource_manager/` | Allocates command buffers and GPU buffers, and manages descriptor sets. |
| `pipeline/` | Builds and caches Vulkan graphics pipelines and their layouts. |
| `renderers/` | Implements shadow-map, HDR rasterization, presentation, quad, particle, and ImGui rendering. |
| `graphics_renderable.*` | Holds graphics-owned state and resources for one immutable renderable definition. |
| `graphics_engine_texture*` | Loads, uploads, samples, and owns texture and derived environment-lighting resources. |
| `environment_map_processor.*` | Converts an sRGB skybox into linear diffuse irradiance, prefiltered specular, and BRDF lookup data. |
| `texture_compositor.*` | Generates immutable GPU-only base-colour textures from ordered texture layers. |
| `video_recorder.*` | Encodes fixed-rate video from bounded, asynchronous per-frame image capture. |
| `raytracing.*` | Dormant implementation excluded from supported build and execution paths. |

Most submodules inherit `GraphicsEngineBaseModule`, which provides controlled
access to the owning `GraphicsEngine` and its shared Vulkan services.

## Frame flow

```text
GameEngine completed-frame publication
        |
        v
GraphicsEngine::run
  |-- acquire and retain the newest completed snapshot
  |-- reconcile immutable renderable/skeleton membership and draw lists
  |-- retire newly unused mesh and material allocations
  |-- update GUI
  `-- SwapChain / GraphicsEngineFrame::draw
       |-- wait for the frame fence and complete its submission serial
       |-- generate utility texture compositions
       |-- record renderer commands
       |-- acquire the swap-chain image
       |-- update per-frame uniforms
       |-- submit to the graphics queue and assign a submission serial
       `-- present on the present queue
```

Normal command recording generates pending utility texture compositions first,
then runs the point-light shadow-map pass when lighting is enabled. Scene
geometry, overlays, and particles render into the linear RGBA16F raster target.
The presentation pass applies exposure and ACES-inspired tone mapping into the
sRGB swap-chain image, followed by the diagnostic quad and ImGui. Screenshot
copy occurs after presentation and before ImGui; recording copy occurs after
ImGui. Every scene pass reads the single snapshot retained for that graphics
iteration.

Recording uses a deterministic session rendezvous between the game and graphics
threads. The game advances by exactly `1 / recording_fps`, publishes one
immutable frame, and waits until graphics has copied that exact frame. Encoder
and rendering stalls therefore extend wall-clock capture time rather than the
video timeline. Outside recording, publication remains non-blocking and
latest-wins. F2 toggles recording globally; the debug panel selects 15–60 FPS.

PBR uploads one glTF-native material record per lit renderable. Factor-only
static and skinned meshes retain material-texture-free pipelines; textured
counterparts use separate pipelines with optional base-colour,
metallic-roughness, normal, and emissive maps. Missing slots bind shared neutral
images and flags skip their samples. All lit paths share the metallic-roughness
BRDF and globally bound IBL resources derived from the active skybox. Direct
point-light radiance uses the published light colour and intensity with
inverse-square attenuation; the shadow map supplies visibility and full
occlusion contributes no direct light. Environment light remains unshadowed.
Material shaders produce unclamped scene-linear output and leave exposure, tone
mapping, and display encoding to the presentation pass.

Renderable shading policy (`Lit` or `Unlit`) is independent of pass modifiers
such as selection stencil and wireframe. Pipeline identity carries both
dimensions, allowing an unlit selected body to use the post-stencil depth policy
without reverting to lit shading. The global Unlit Base Color mode overrides
scene geometry through the same unlit pipeline family; renderables explicitly
marked unlit remain unlit even in other global debug modes. Unlit renderables do
not contribute to shadow draw lists.

## Deferred GPU resource retirement

Each graphics-queue submission receives a monotonically increasing serial
associated with its frame fence. Waiting for a frame's fence before reuse
advances the completed serial. Graphics-queue ordering means that completing a
serial also completes every earlier graphics submission.

Replacing a renderable or skeleton creates a new immutable identity. Future
commands stop using the removed identity immediately, while its exact buffer
and descriptor allocations move to a retirement batch tagged with the latest
submitted serial. Unused mesh and material graphics allocations use the same
mechanism. A batch is released only after its serial completes, allowing old
and replacement identities to coexist while frames remain in flight.

Ordinary renderable/skeleton topology reconciliation and unused-resource cleanup
therefore do not call `vkDeviceWaitIdle()`. Replacing the single global
environment is the deliberate exception: it waits before rewriting descriptor
sets that may still be in flight. Per-renderable uniform-buffer and
descriptor-pool capacity allows for the active topology plus one retired
resource set per possible in-flight swap-chain image. Shutdown retains the
device-wide wait and then flushes all retirement batches. One-time staging and
upload commands use their own queue synchronization rather than this retirement
mechanism.

## Data ownership and boundaries

- The game/ECS side is the mutable source of truth and publishes complete
  renderable state through `RenderFrame`.
- `GraphicsEngine` depends on the window and graphics-owned resources, not on
  `GameEngine`, mutable `Object` instances, the ECS, or the game camera.
- One graphics renderable is owned and reconciled per immutable `RenderableID`.
  It owns its transform-buffer slot, material and texture descriptors, and
  other per-instance GPU state. Dynamic state comes from the accepted snapshot.
- Cached flat draw lists classify and order each renderable independently for
  each scene pass. CPU mesh/material ownership and graphics buffer/texture
  allocations remain shared through their respective managers.
- Skeleton graphics resources are keyed independently by `SkeletonID`. Each
  skeleton has one bone-buffer slot per swap-chain frame, shared by all bound
  renderables.
- `RenderableID` and `SkeletonID` definitions cannot change or be reintroduced.
  Structural replacement creates a new ID; the render-frame mailbox rejects
  producers that violate this contract.
- The latest-wins mailbox lets either thread advance independently during
  normal rendering. A slow graphics loop drops intermediate publications
  rather than blocking updates. Deterministic recording temporarily gates the
  producer after publication until that frame has been copied.
- Each snapshot includes render mode and stencil-selection state, so dropping
  an intermediate publication cannot lose a state transition. Stencil entries
  are `ObjectID` grouping metadata, not graphics-resource ownership keys.
- Graphics shutdown uses a dedicated atomic stop request rather than frame
  state.
- Swap-chain frames own transient synchronization and command resources.
  Long-lived device resources belong to their manager or renderer and transfer
  to serial-gated retirement batches when superseded.

The complete game-to-renderer contract is described in
[RENDER_BOUNDARY.md](RENDER_BOUNDARY.md).

## Where to start

- [`graphics_engine.hpp`](../src/graphics_engine/graphics_engine.hpp) and
  [`graphics_engine.cpp`](../src/graphics_engine/graphics_engine.cpp) define
  lifecycle and ownership.
- [`graphics_engine_frame.ipp`](../src/graphics_engine/graphics_engine_frame.ipp)
  defines recording, synchronization, capture, and pass ordering.
- [`renderer_manager.ipp`](../src/graphics_engine/renderers/renderer_manager.ipp)
  defines the installed renderer set and renderer wiring.
- [`graphics_resource_manager.hpp`](../src/graphics_engine/resource_manager/graphics_resource_manager.hpp)
  is the entry point for command-buffer, buffer, and descriptor allocation.
