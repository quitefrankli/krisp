# Graphics Engine Architecture

`GraphicsEngine` is Krisp's Vulkan-backed rendering boundary. The game layer
owns mutable scene objects and publishes completed immutable snapshots; this
module owns the window-facing Vulkan objects, GPU representations, and render
loop.

## Major components

| Area | Responsibility |
| --- | --- |
| `graphics_engine.*` | Top-level coordinator: owns the render loop, queues, graphics objects, and submodules. |
| `graphics_engine_instance/device/swap_chain.*` | Creates the Vulkan instance, surface, device, queues, swap chain, and per-swap-chain frames. |
| `graphics_engine_frame.*` | Records and submits one frame's command buffer; owns its fences, semaphores, and per-frame resources. |
| `resource_manager/` | Allocates command buffers and GPU buffers, and manages descriptor sets. |
| `pipeline/` | Builds and caches Vulkan graphics/compute pipelines and their layouts. |
| `renderers/` | Implements rendering passes such as shadow maps, rasterization, ray tracing, compositing, particles, and ImGui. |
| `graphics_engine_object.*` | Holds graphics-owned state for one immutable render-object definition. |
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
  ├─ reconcile versioned graphics-owned definitions when needed
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
- `GraphicsEngineObject` retains an immutable versioned definition. Membership
  and definition changes reconcile graphics resources; dynamic state is read
  directly from the accepted snapshot.
- The latest-wins mailbox lets either thread advance independently. A slow
  graphics loop drops intermediate publications instead of blocking updates.
- The command queue carries controls only; stencil commands carry an
  `ObjectID`, not a game-object reference.
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
