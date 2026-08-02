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
| `renderers/` | Implements shadow-map, rasterization, quad/compositing, particle, and ImGui rendering. |
| `graphics_renderable.*` | Holds graphics-owned state and resources for one immutable renderable definition. |
| `graphics_engine_texture*` | Loads, uploads, samples, and owns texture resources. |
| `texture_compositor.*` | Generates immutable GPU-only base-colour textures from ordered texture layers. |
| `video_recorder.*` | Manages video recording fed by per-frame image capture. |
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
       |-- generate newly introduced texture compositions
       |-- record renderer commands
       |-- acquire the swap-chain image
       |-- update per-frame uniforms
       |-- submit to the graphics queue and assign a submission serial
       `-- present on the present queue
```

Normal command recording generates pending texture compositions first, then
runs the shadow-map pass when lighting is enabled, followed by rasterization,
quad/post-processing, and ImGui. Particle rendering records
inside the rasterization pass rather than using a separate pass. Screenshot
capture is prepared after compositing and omits ImGui for that frame; recording
capture is prepared after ImGui. Every scene pass reads the single snapshot
retained for that graphics iteration.

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

Topology reconciliation and unused-resource cleanup therefore do not call
`vkDeviceWaitIdle()`. Per-renderable uniform-buffer and descriptor-pool capacity
allows for the active topology plus one retired resource set per possible
in-flight swap-chain image. Shutdown retains the device-wide wait and then
flushes all retirement batches. One-time staging and upload commands use their
own queue synchronization rather than this retirement mechanism.

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
- The latest-wins mailbox lets either thread advance independently. A slow
  graphics loop drops intermediate publications rather than blocking updates.
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
