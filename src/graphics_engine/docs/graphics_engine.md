# Graphics Engine Documentation

The graphics engine aims to abstract away the specific graphics api from the rest of the code, in this case it would be Vulkan.

It's supplemented with "submodules", which are sub components of the graphics engine i.e. GraphicsResourceManager, PipelineManager.

Most submodules inherit from `GraphicsEngineBaseModule`, which provides access
to the owning graphics engine and its shared Vulkan services.

## Submodules

### RendererManager

Holds the renderer set. The shadow-map, rasterization, quad, and ImGui renderers
own their pass-specific resources for every in-flight frame. Particle rendering
records inside the rasterization pass and reuses that render pass rather than
owning a separate one.

Frame synchronization and shared uniform updates belong to
`GraphicsEngineFrame`; renderers own only their pass-specific resource updates
and command recording.

`GraphicsEngineFrame` records the passes in order by calling each active
renderer with the frame command buffer. Each renderer appends its commands to
that buffer.

## Frame synchronization and resource retirement

Each graphics-queue frame submission is assigned a monotonically increasing
serial and associated with that frame's fence. When a frame fence is waited
before reuse, the graphics engine advances its completed submission serial.
Because submissions execute in queue order, completing a serial also completes
all earlier graphics submissions.

Replacing a renderable or skeleton creates a new immutable identity. Newly
recorded commands stop using the removed identity, but its resources are not
destroyed immediately. Their exact buffer and descriptor allocations are moved
into a retirement batch tagged with the latest submitted serial. Unused mesh,
material, and texture graphics allocations use the same mechanism. Batches are
released only when their serial is complete, so removed and replacement
identities can coexist safely while frames remain in flight.

Normal topology reconciliation and unused-resource cleanup do not call
`vkDeviceWaitIdle()`. Device-wide waiting remains part of graphics-engine
shutdown, after which all pending retirement batches can be flushed safely.
One-time staging and upload commands retain their separate queue
synchronization.
