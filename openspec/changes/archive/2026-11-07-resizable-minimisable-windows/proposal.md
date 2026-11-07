# Proposal

## Why

The native window allows resizing and minimising, but rendering assumes its initial dimensions and cannot recover reliably when the surface changes. Supporting these ordinary window operations requires explicit resource lifetimes rather than enabling callbacks alone.

## What Changes

- Require KHR or EXT swapchain maintenance support for presentation completion fences, with explicit startup rejection when unavailable.
- Recover from framebuffer size changes and out-of-date/suboptimal swapchains.
- Suspend rendering while minimised or the framebuffer is zero-sized; continue simulation, audio and event processing.
- Replace size-dependent GPU resources safely and use dynamic viewport/scissor state.
- Update camera projections and preserve correct cursor picking and UI rendering.
- Stop active video recording cleanly when framebuffer dimensions change.
- Compare unchanged-window performance against the existing pre-change baselines, preserving their documented variance limits.

## Capabilities

### New Capabilities

- `window-presentation-lifecycle`: Window resize, minimise/restore, surface recovery, camera sizing and capture lifecycle behaviour.

### Modified Capabilities

None. Benchmark protocols and rendering fidelity requirements remain unchanged.

## Impact

Window event publication, game/graphics thread coordination, swapchain/frame lifetime, renderers, per-image buffers/descriptors, pipelines, camera, UI and capture paths. No new application dependencies or resolution scaling system. Ray tracing stays disabled. Internal fixed-extent interfaces may change without compatibility shims.
