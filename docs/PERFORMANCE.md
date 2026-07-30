# Performance Notes

Record performance-sensitive implementation choices here. Include measured
results when available; otherwise distinguish expected costs from measurements.

## Point-light shadow filtering

Rasterized point-light shadows use 16-tap Poisson-disc PCF in
`shaders/library/library.glsl`. The helper is shared by the color, texture,
skinned-color, and skinned-texture pipelines.

### Cost

- Each lit fragment performs 16 cubemap depth samples. These texture reads are
  expected to dominate the filter cost.
- Moving from 9 to 16 taps increased shadow texture reads and per-tap arithmetic
  by 78%. This is a theoretical increase, not a frame-time measurement.
- Each tap also normalizes its lookup direction and performs receiver-plane
  depth correction. This avoids concentric self-shadowing on large flat
  receivers, at additional arithmetic cost.
- Textured materials use the geometric normal for shadow correction. Normal-map
  perturbations remain limited to lighting and do not destabilize shadow depth.

### Tuning

- Reducing the Poisson sample count is the most direct quality/performance
  tradeoff. Keep the loop bound and visibility divisor synchronized with the
  kernel size.
- `texel_size` controls a one-texel filter radius relative to the cubemap face
  resolution. Increasing it softens edges but makes undersampling more visible.
- If profiling identifies receiver-plane arithmetic as significant, calculate
  loop-invariant values outside the loop and assess whether normalization can
  be avoided without reintroducing artifacts.

No GPU timing measurements have been recorded yet.

## Procedural resource factories

Mesh and material factory calls create independently owned resources rather
than using process-wide caches. Repeated calls therefore repeat mesh generation
or material allocation and consume separate graphics resources. Callers that
need reuse should retain and share the returned owner instead of invoking a
factory and registering its result each frame. Factories only construct CPU
resources; callers explicitly register them with `MeshSystem` or
`MaterialSystem`. No timing measurements have been recorded.

## Renderable-local transforms

Rasterization keeps imported asset-node transforms separate from gameplay
object transforms. Each frame, the CPU precomputes `gameplay * local` into one
uniform-buffer slot per renderable. Static vertex shaders retain their previous
matrix workload.

Skinned bone buffers now remain in model space so they can be shared by
renderables. Skinned vertex shaders apply the combined model matrix after
skinning, adding one matrix-vector transform for positions and equivalent work
for normals/tangents in lit passes. This is an expected cost; no GPU timing
measurements have been recorded. The transform separation also increases
per-frame uniform storage and descriptor-set use in proportion to renderable
count rather than object count.

## Skeletal animation cross-fades

Cross-fades retain one local transform snapshot per bone for the transition and
evaluate the target pose into a temporary per-bone pose each update. This adds
linear CPU work and temporary storage proportional to bone count only while a
fade is active. No measurements have been recorded.

## Bone attachments

Animated prop attachments compose each source skeleton's model-space pose once
per frame, then reuse it for every entity attached to that skeleton. Expected
CPU work and temporary storage are linear in the bone count of skeletons with
attachments, plus constant transform work per attachment. Skeletons without
attachments incur no pose-composition cost. No measurements have been recorded.

Animation pose writes and render-frame pose snapshots are both confined to the
game thread. The graphics thread consumes only the copied pose, so skeletal
processing has no cross-thread mutex or render-thread contention.

## Render-frame publication

The game thread now builds one immutable render snapshot after each update.
Per-frame work is linear in object count, renderable/material-reference count,
attached bone count, and live particle count. Object and skeleton definitions
are compared with cached immutable definitions each update, but their vectors
and asset handles are copied only when definition content changes.

Current and previous frames remain shared-owned by the publication mailbox, so
their dynamic vectors and referenced mesh/material assets remain alive until
consumers release them or newer frames displace them. The graphics thread loads
one publication per loop and retains it for every pass. If rendering is slower
than game updates, intermediate snapshots are discarded by the latest-wins
mailbox instead of queuing work or blocking the producer.

Render definitions expose their retained mesh and material assets through
owner-based const access. Graphics recording and resource upload therefore do
not lock or query the mutable asset registries. Registry synchronization is
limited to ownership lookup/update and the cross-thread retirement handoff.

Graphics-owned objects and their descriptor/buffer topology are reconciled
only when membership or object/skeleton definition versions change. Dynamic
camera, visibility, hierarchy-transform, particle, and bone-pose changes reuse
that topology. Topology changes synchronize the Vulkan device before replacing
affected resources. Released mesh/material GPU allocations are likewise
retired only after graphics synchronization; these waits affect the graphics
thread but never the game publisher.

The unsupported ray-tracing source and shader paths are excluded from builds.
Its renderer, pipeline, descriptor layouts, device extensions/features, and
mapping buffer are not created, reducing fixed startup/device resource
requirements. No timing measurements have been recorded.

## UI layers

Only the UI layer for the active game mode is processed and drawn: engine
panels in editor mode, or application windows and overlays in normal mode.
The FPS/TPS overlay is the sole exception and is processed and drawn in both
modes.
Application UI registration is sealed before the render thread starts, so the
render path reads stable vectors without registry locking. Application windows
are traversed twice to preserve window-before-overlay ordering; this is linear
in the typically small number of application UI elements and has not been
measured.
