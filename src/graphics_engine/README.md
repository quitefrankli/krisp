# Graphics Engine Architecture

`GraphicsEngine` is Krisp's Vulkan-backed rendering boundary. The game layer
owns scene objects and submits graphics commands; this module owns the Vulkan
objects, GPU-facing representations, and the render loop that presents frames.

## Major components

| Area | Responsibility |
| --- | --- |
| `graphics_engine.*` | Top-level coordinator: owns the render loop, queues, graphics objects, and submodules. |
| `graphics_engine_instance/device/swap_chain.*` | Creates the Vulkan instance, surface, device, queues, swap chain, and per-swap-chain frames. |
| `graphics_engine_frame.*` | Records and submits one frame's command buffer; owns its fences, semaphores, and per-frame resources. |
| `resource_manager/` | Allocates command buffers and GPU buffers, and manages descriptor sets. |
| `pipeline/` | Builds and caches Vulkan graphics/compute pipelines and their layouts. |
| `renderers/` | Implements rendering passes such as shadow maps, rasterization, ray tracing, compositing, particles, and ImGui. |
| `graphics_engine_object.*` | Holds GPU-side state for a game object and its renderables. |
| `graphics_engine_texture*` | Loads, uploads, samples, and owns texture resources. |
| `raytracing.*` | Maintains ray-tracing acceleration structures and related resources. |

Most submodules inherit `GraphicsEngineBaseModule`, which provides controlled
access back to the owning `GraphicsEngine` and its shared Vulkan services.

## Frame flow

```text
GameEngine commands
        |
        v
GraphicsEngine::run
  ├─ process queued graphics commands
  ├─ update GUI and ray-tracing state
  └─ SwapChain / GraphicsEngineFrame::draw
       ├─ wait for the frame fence
       ├─ record renderer command passes
       ├─ acquire a swap-chain image
       ├─ update per-frame uniforms
       ├─ submit to the graphics queue
       └─ present on the present queue
```

With rasterization enabled, command recording executes the shadow-map pass,
rasterization pass, and quad/composite pass before the ImGui pass. With ray
tracing enabled, the ray-tracing renderer replaces that scene-pass sequence.
Particles are recorded by the rasterization renderer.

## Data ownership and boundaries

- The game/ECS side remains the source of truth for objects, meshes,
  materials, camera state, and scene changes.
- `GraphicsEngineObject` and the resource managers mirror only the GPU state
  required to render those objects.
- Changes cross the boundary through the graphics-engine command queue, which
  the render thread drains at the start of each loop iteration.
- Swap-chain frames own transient per-frame synchronization and command
  resources; long-lived device resources belong to the relevant manager or
  renderer.

## Where to start

- Begin at [`graphics_engine.hpp`](graphics_engine.hpp) and
  [`graphics_engine.cpp`](graphics_engine.cpp) for lifecycle and ownership.
- Read [`graphics_engine_frame.ipp`](graphics_engine_frame.ipp) for frame
  recording, synchronization, and pass ordering.
- See [`renderers/renderer_manager.ipp`](renderers/renderer_manager.ipp) for
  the installed renderer set and renderer wiring.
- See [`docs/graphics_engine.md`](docs/graphics_engine.md) for the original
  subsystem notes.
