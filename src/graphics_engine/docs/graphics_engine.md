# Graphics Engine Documentation

The graphics engine aims to abstract away the specific graphics api from the rest of the code, in this case it would be Vulkan.

It's supplemented with "submodules", which are sub components of the graphics engine i.e. GraphicsResourceManager, PipelineManager.

Each submodule inherit from a BaseSubModule that features a nice interface that gives access to most of the graphics engine and other components.

## Submodules

### RendererManger

Holds a collection of renderers, each renderer has its own renderpass. Since we are using multiple in flight frames, each renderer is also required to hold multiple sets of attachments and framebuffers.

The renderers themselves aren't responsible for synchronisation or uniform buffer updates e.t.c. these are all done by each swapchain frame (which is also responsible for calling the renderers' draw method)

The renderers request rendering by calling each renderer and providing a `command_buffer` that each renderer then submits commands into

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
