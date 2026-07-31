# Graphics Engine Architecture

`GraphicsEngine` is Krisp's Vulkan-backed rendering boundary. The game layer
owns mutable scene state and publishes completed immutable snapshots; this
module owns the window-facing Vulkan objects, GPU representations, and render
loop.

## Major components

| Area | Responsibility |
| --- | --- |
| `graphics_engine.*` | Top-level coordinator: owns the render loop, queues, graphics renderables, and submodules. |
| `graphics_engine_instance/device/swap_chain.*` | Creates the Vulkan instance, surface, device, queues, swap chain, and per-swap-chain frames. |
| `graphics_engine_frame.*` | Records and submits one frame's command buffer; owns its fences, semaphores, and per-frame resources. |
| `render_draw_list.*` | Caches renderable-level pass classification and state ordering for reconciled topology. |
| `resource_manager/` | Allocates command buffers and GPU buffers, and manages descriptor sets. |
| `pipeline/` | Builds and caches Vulkan graphics/compute pipelines and their layouts. |
| `renderers/` | Implements rendering passes such as shadow maps, rasterization, ray tracing, compositing, particles, and ImGui. |
| `graphics_renderable.*` | Holds graphics-owned state and resources for one immutable renderable definition. |
| `graphics_engine_texture*` | Loads, uploads, samples, and owns texture resources. |
| `raytracing.*` | Dormant implementation; ray tracing is unsupported and excluded from builds. |

Most submodules inherit `GraphicsEngineBaseModule`, which provides controlled
access back to the owning `GraphicsEngine` and its shared Vulkan services.

## Frame flow

```text
GameEngine completed-frame publication
        |
        v
GraphicsEngine::run
  ├─ process control-only graphics commands
  ├─ acquire and retain the newest completed snapshot
  ├─ reconcile versioned graphics-owned definitions and draw lists when needed
  ├─ retire released GPU assets after graphics synchronization
  ├─ update GUI
  └─ SwapChain / GraphicsEngineFrame::draw
       ├─ wait for the frame fence
       ├─ record renderer command passes
       ├─ acquire a swap-chain image
       ├─ update per-frame uniforms
       ├─ submit to the graphics queue
       └─ present on the present queue
```

Command recording executes the shadow-map, rasterization, and quad/composite
passes before the ImGui pass. Particles are recorded by the rasterization
renderer. Every scene pass reads the one snapshot retained for that graphics
iteration. Ray tracing is currently unsupported.

## Data ownership and boundaries

- The game/ECS side remains the mutable source of truth and publishes all
  renderable state through `RenderFrame`.
- `GraphicsEngine` depends on the window and graphics-owned resources, never
  on `GameEngine`, mutable `Object` instances, the ECS, or the game camera.
- One graphics renderable is owned and reconciled per persistent
  `RenderableID`. It retains an immutable versioned definition and owns its
  transform-buffer slot, material and texture descriptors, and other per-instance
  GPU state. Dynamic state is read directly from the accepted snapshot.
- Cached flat draw lists classify and order each renderable independently for
  each scene pass. CPU mesh/material ownership and graphics buffer/texture
  allocations remain shared through their respective managers.
- Skeleton graphics resources are keyed independently by `SkeletonID`. Each
  skeleton has one bone-buffer slot per swap-chain frame, shared by all renderables
  bound to it.
- The latest-wins mailbox lets either thread advance independently. A slow
  graphics loop drops intermediate publications instead of blocking updates.
- The command queue carries controls only; stencil commands carry an
  `ObjectID`, not a game-object reference. This ID is optional renderable
  grouping metadata rather than a graphics-resource ownership key.
- Swap-chain frames own transient per-frame synchronization and command
  resources; long-lived device resources belong to the relevant manager or
  renderer.

The full boundary contract is documented in
[`docs/RENDER_BOUNDARY.md`](../../docs/RENDER_BOUNDARY.md).

## Where to start

- Begin at [`graphics_engine.hpp`](graphics_engine.hpp) and
  [`graphics_engine.cpp`](graphics_engine.cpp) for lifecycle and ownership.
- Read [`graphics_engine_frame.ipp`](graphics_engine_frame.ipp) for frame
  recording, synchronization, and pass ordering.
- See [`renderers/renderer_manager.ipp`](renderers/renderer_manager.ipp) for
  the installed renderer set and renderer wiring.
- See [`docs/graphics_engine.md`](docs/graphics_engine.md) for the original
  subsystem notes.
