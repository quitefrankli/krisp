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
| `graphics_engine_instance/device/swap_chain.*` | Creates the Vulkan instance, surface, device, GPU allocator, queues, swap chain, and per-swap-chain frames. |
| `graphics_engine_frame.*` | Records and submits one frame's command buffer; owns its fences, semaphores, and transient per-frame resources. |
| `render_draw_list.*` | Caches renderable-level pass classification and state ordering for reconciled topology. |
| `resource_manager/` | Allocates command buffers and GPU buffers, and manages descriptor sets. |
| `pipeline/` | Builds and caches Vulkan graphics pipelines and their layouts. |
| `renderers/` | Implements shadow-map, HDR rasterization, presentation, quad, particle, and ImGui rendering. |
| `graphics_renderable.*` | Holds graphics-owned state and resources for one immutable renderable definition. |
| `graphics_engine_texture*` | Loads, uploads, samples, and owns texture and derived environment-lighting resources. |
| `environment_map_processor.*` | Converts an sRGB skybox into linear diffuse irradiance, prefiltered specular, and BRDF lookup data. |
| `environment_map_asset.*` | Reads and writes the versioned, source-validated offline representation of those derived environment textures. |
| `texture_compositor.*` | Generates immutable GPU-only base-colour textures from ordered texture layers. |
| `video_recorder.*` | Encodes fixed-rate video from bounded, asynchronous per-frame image capture. |
| `raytracing.*` | Dormant implementation excluded from supported build and execution paths. |

Most submodules inherit `GraphicsEngineBaseModule`, which provides controlled
access to the owning `GraphicsEngine` and its shared Vulkan services.

## GPU memory

Engine-owned buffers and images use Vulkan Memory Allocator (VMA). The device
module creates one allocator after the logical device and destroys it after all
resource-owning modules, before destroying the device. Swap-chain images remain
owned by Vulkan and are never freed through VMA.

`GraphicsBuffer` owns its buffer and allocation, transfers ownership on move,
and releases them on destruction. Slot allocation within the engine's large
vertex, index, uniform, and storage buffers remains separate from VMA's device
memory suballocation. Image owners retain a `VmaAllocation` alongside their
image and release image views before destroying the image allocation.

Allocation requests preserve required memory properties and use VMA's automatic
memory selection. Upload mappings request sequential host writes; readback
mappings request random host access and prefer cached memory. CPU writes are
flushed before GPU use; CPU readbacks invalidate after the submission fence has
completed. VMA handles allocation offsets and non-coherent cache alignment.
Mapping does not replace submission synchronization or resource retirement.
See the [VMA memory mapping documentation](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html).

## Vulkan synchronization

The device requires and enables Vulkan 1.3 `synchronization2` alongside
`dynamicRendering`. Image dependencies use `VkImageMemoryBarrier2` through the
shared `command_image_barrier` helper, which records `vkCmdPipelineBarrier2`.
Each barrier carries its own 64-bit stage and access masks, layer range, and
mip range. Copy operations use the copy stage; texture sampling uses sampled
read access. Empty scopes use `NONE` rather than top/bottom-of-pipe barriers.
Discarding an attachment's contents does not remove dependencies on prior uses.

Frame and one-time upload submissions use `vkQueueSubmit2`. Image acquisition
waits at color attachment output, matching the first swap-chain image
transition. The render-finished binary semaphore signals after all commands,
including capture copies and timestamp writes, before presentation. Fences,
resource retirement, and one-time upload queue-idle waits retain their existing
roles. GPU observation uses `vkCmdWriteTimestamp2` at the same measurement
stages as before; top/bottom-of-pipe stages remain intentional timestamp
endpoints rather than synchronization barriers.

See the [Khronos Synchronization2 guide](https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html).

With synchronization validation enabled, the installed 1.3.275 layer also
reports `PRESENT_AFTER_WRITE` for the final present-layout transition followed
by an `ALL_COMMANDS` semaphore signal. This sequence matches the documented
[upstream validation false positive](https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/7479),
fixed by [Vulkan-ValidationLayers #7480](https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/7480).
Standard Vulkan validation and all 11 procedural pixel comparisons pass with
the Synchronization2 paths.

## Dynamic rendering

The renderer requires Vulkan 1.3 with the `dynamicRendering` and
`synchronization2` features enabled.
Graphics pipelines declare color, depth, and stencil attachment formats through
`VkPipelineRenderingCreateInfo`. Renderers bind image views directly with
`vkCmdBeginRendering`; supported paths do not allocate `VkRenderPass` or
`VkFramebuffer` objects.

Attachment layout transitions and producer/consumer visibility use explicit
image barriers. The scene keeps its multisampled HDR color and depth/stencil
attachments and resolves to a single-sample image for presentation. Shadow
rendering binds the full depth array with a layer count covering its cube faces.
Presentation and GUI return the swap-chain image to `PRESENT_SRC_KHR`, preserving
the capture points between passes.

Migration verification compared all 11 procedural fidelity captures exactly
against the prior renderer. Standard Vulkan validation was clean. With
synchronization validation enabled, the installed 1.3.275 validation layer
reports a shadow-image read-after-layout-transition warning when initialized
zero-shadow frames skip rendering. The same warning was reproduced with the
prior render-pass implementation; its cause remains unresolved.

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
then runs the point-light shadow-map pass. Scene geometry, overlays, and
particles render into the linear RGBA16F raster target.
The presentation pass applies exposure and ACES-inspired tone mapping into the
selected swap-chain image, followed by the diagnostic quad and ImGui. Fidelity
capture copies immediately after presentation, before diagnostic quads and UI.
The debug screenshot copies after the diagnostic quad and before ImGui;
recording copy occurs after ImGui. Every scene pass reads the single snapshot
retained for that graphics iteration.

Recording uses a deterministic session rendezvous between the game and graphics
threads. The game advances by exactly `1 / recording_fps`, publishes one
immutable frame, and waits until graphics has copied that exact frame. Encoder
and rendering stalls therefore extend wall-clock capture time rather than the
video timeline. Outside recording, publication remains non-blocking and
latest-wins. F2 toggles recording globally; the debug panel selects 15–60 FPS.

An optional `FrameCaptureSession` accepts one bounded request at a time. A
request binds to the next completed immutable publication; the game thread
waits after publishing it, while graphics copies only when the accepted frame
identity matches. The result includes frame identity, extent, source Vulkan
format and colour space, and canonical top-to-bottom RGBA8 bytes. Readback
completes after the submission fence. Supported source formats are RGBA8 and
BGRA8 in UNORM or sRGB variants; other formats resolve as failures. Cancellation
and shutdown resolve a pending request. Fence, mapping, submission, and request
deadline failures return explicit failed results; callers can also cancel
independently of their finite wait deadline.
The caller is responsible for making resource upload/composition readiness
explicit before requesting capture and for holding fixed scene state while the
request resolves.

PBR uploads one glTF-native material record per lit renderable. Factor-only
static and skinned meshes retain material-texture-free pipelines; textured
counterparts use separate pipelines with optional base-colour,
metallic-roughness, normal, and emissive maps. Missing slots bind shared neutral
images and flags skip their samples. All lit paths share the metallic-roughness
BRDF and globally bound IBL resources derived from the active skybox. Direct
point-light radiance uses the published light colour and intensity with
inverse-square attenuation. Up to eight point lights are copied into each
render snapshot in ascending object-ID order; disabled lights still occupy a
slot. Shared fragment shading sums every enabled light, skips lights with zero
intensity or black colour, and skips shadow sampling on back-facing contributions. Each enabled
shadow-casting light with nonzero radiance receives a dense cube-array slot.
The shadow pass renders each eligible shadow caster into six faces per such
light, excluding the light's own object. Lit fragments apply 16-tap PCF to
shadowed contributions; full occlusion removes that light's direct
contribution. Environment light remains unshadowed.
Material shaders produce unclamped scene-linear output and leave exposure, tone
mapping, and display encoding to the presentation pass.

Shadow cube arrays are owned per swap-chain frame. Their capacity grows to the
next power of two when that frame needs more shadow slots and remains at its
high-water capacity until shutdown. Resource replacement updates only the
descriptor for the frame whose fence has completed. Before a frame first needs
shadows, it uses a bindable 1-by-1 cube array. Grown arrays remain allocated
when lights are subsequently disabled or removed. Growth needs no device-wide idle wait.
The renderer exposes the first cube's first face through a 2D preview view;
each frame updates its own quad descriptor after its fence completes.

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
- An explicit frame-capture request gates publication at its selected immutable
  frame until fence-complete RGBA8 readback is available. Normal rendering has
  no capture staging allocation or extra image copy.
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

## Window presentation state

The event thread publishes framebuffer pixel dimensions, iconification and a
monotonic generation through `Window::get_presentation_state()`. GLFW input
callbacks retain their engine callback target separately from the window user
pointer. Logical window dimensions remain the coordinate basis for cursor
normalisation; zero dimensions produce a neutral cursor position.

Camera aspect updates run on the game thread before immutable scene publication.
They preserve perspective vertical field of view and orthographic vertical span.
Graphics pipelines use dynamic viewport/scissor state set once per pass, using
that pass's extent (including separate shadow and preview extents).

The graphics thread coalesces framebuffer generations and rebuilds presentation
resources after draining graphics and presentation fences. KHR or EXT
`swapchain_maintenance1` with its surface dependency is required; unsupported
devices fail startup explicitly. Scene assets and compatible pipelines survive
recreation; fixed-size shadow, particle and preview resources survive when the
image count stays the same. Per-image renderable/skeleton resources are rebuilt
if the actual image count changes.

Iconification or a zero framebuffer suspends acquisition, drawing and presentation
on a condition variable. Window events, simulation and audio continue; shutdown
wakes the graphics thread. Recording output pauses during minimisation and resumes
without synthesising frames. A framebuffer size change finalises and stops an
active recording cleanly. Submitted captures drain at their original dimensions;
unsubmitted captures fail explicitly on recreation or suspension. New frame
capture requests while suspended fail immediately through the existing result
slot. UI preview descriptors refresh after their image resources are replaced. The
bundled ImGui Vulkan backend updates its render-buffer image count without
shutting down the GLFW platform backend; unrelated texture descriptors remain
valid. Empty descriptor batches are handled without a Vulkan allocation call.

`GuiDebug::request_screenshot()` posts a thread-safe screenshot request through
the same path as the debug UI. Pending screenshots carry their submitted extent
through readback and PNG writing. Recording finalisation checks the actual new
surface extent, which can race ahead of GLFW events. Shutdown drains captures
while the recorder and UI still exist; late swapchain destruction only waits for
Vulkan completion and does not call already-destroyed engine services.

Native lifecycle checks are available as `window_lifecycle_integration`; see
PERFORMANCE.md for build/run commands, fixture modes and measured limitations.
