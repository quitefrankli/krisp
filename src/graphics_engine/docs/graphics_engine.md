# Graphics Engine Documentation

The graphics engine aims to abstract away the specific graphics api from the rest of the code, in this case it would be Vulkan.

It's supplemented with "submodules", which are sub components of the graphics engine i.e. GraphicsResourceManager, PipelineManager.

Each submodule inherit from a BaseSubModule that features a nice interface that gives access to most of the graphics engine and other components.

## Submodules

### RendererManger

Holds a collection of renderers, each renderer has its own renderpass. Since we are using multiple in flight frames, each renderer is also required to hold multiple sets of attachments and framebuffers.

The renderers themselves aren't responsible for synchronisation or uniform buffer updates e.t.c. these are all done by each swapchain frame (which is also responsible for calling the renderers' draw method)

The renderers request rendering by calling each renderer and providing a `command_buffer` that each renderer then submits commands into


