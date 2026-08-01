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
resources; callers explicitly register them with the mesh or material store
owned by their ECS. No timing measurements have been recorded.

## Renderable-local transforms

Rasterization keeps imported asset-node transforms separate from gameplay
object transforms. Each frame, the CPU precomputes the optional group's world
transform multiplied by the renderable-local transform. A standalone
renderable's local transform is already world-relative. The result is uploaded
to the shared transform-buffer slot keyed by that `RenderableID`. Static vertex shaders
retain their previous matrix workload.

Skinned bone buffers now remain in model space so they can be shared by
renderables. Skinned vertex shaders apply the combined model matrix after
skinning, adding one matrix-vector transform for positions and equivalent work
for normals/tangents in lit passes. This is an expected cost; no GPU timing
measurements have been recorded. The transform separation also increases
per-frame uniform storage and descriptor-set use in proportion to renderable
count rather than object count. This is deliberate: the renderable instance is
the independently replaceable graphics-resource unit.

## Renderable draw lists

Graphics renderables are the ownership and dynamic-state boundary. Topology
reconciliation builds flat draw-item lists for rasterization and processes
immutable membership per `RenderableID`; replacing one attachment with a new ID
does not recreate its former object-group peers. Opaque, masked, overlay, and
shadow lists are classified and state-sorted only when renderable or skeleton
topology changes. Blended main and overlay items are sorted back-to-front each
graphics frame using the published world transform, with `RenderableID` as the
deterministic tie-breaker.

The state order groups pipelines, meshes, and material identities. Command
recording skips redundant pipeline and vertex/index-buffer binds within a
render pass; per-draw transform and material descriptor sets are still bound
for every item. This is expected to reduce CPU command-recording work and
driver state processing for scenes with repeated state, but no timing
measurements have been recorded.

## Shared skeleton graphics resources

Skeleton pose and topology are independent from renderable ownership. The
graphics engine allocates and updates one bone-buffer slot per `SkeletonID` and
swap-chain frame, then binds it from every renderable attached to that
skeleton. Multiple skinned mesh primitives or instances can therefore share a
single pose upload instead of duplicating bone buffers and updates per
renderable. Descriptor reconciliation remains proportional to the number of
new bindings when a skeleton identity is replaced. No timing measurements have
been recorded.

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

## Transformation hierarchy

The ECS owns local transforms and parent-child links in one transformation
registry. World transforms are composed lazily and cached; changing a transform
or hierarchy link marks only that entity and its descendants dirty. Repeated
world-transform reads therefore reuse the cached result until an ancestor
changes. Registry access adds an entity-ID hash lookup compared with the former
direct `Object` storage. Components are stored directly in the hash registry;
the registry therefore owns their storage without an extra allocation or
pointer indirection per component. No timing measurements have been recorded.

## Render-frame publication

The game thread now builds one immutable render snapshot after each update.
Per-frame work is linear in renderable/material-reference count, published
skeleton-pose size, attached bone count, and live particle count. Cached
renderable and skeleton definitions are checked each update to enforce that a
definition never changes for an existing ID; structural replacement instead
creates a new ID. Grouped renderables perform object lookups to compose
transforms, visibility, and other object-level behavior.

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

Graphics-owned renderables and their descriptor/buffer topology are reconciled
only when immutable renderable or skeleton membership changes. Dynamic camera,
visibility, transform, particle, and bone-pose changes reuse that topology.
Per-renderable reconciliation limits replacement to the changed identity;
shared skeleton buffers avoid repeated pose uploads for every referencing
renderable.

Topology replacement and unused mesh/material cleanup do not call
`vkDeviceWaitIdle()`. Each graphics-queue submission receives a monotonically
increasing serial associated with its frame fence. Once that fence has been
waited, the completed serial advances; graphics-queue ordering also establishes
that all earlier serials have completed. Resources removed from current
topology are placed in a retirement batch tagged with the latest submitted
serial and released only after that serial is complete.

Removed and replacement renderable or skeleton identities can consequently
coexist while an earlier frame still references the removed identity. This
avoids a device-wide runtime stall during frequent topology changes, at the
expected cost of temporarily retaining superseded GPU allocations. The
per-renderable uniform buffer and descriptor pool include capacity for the
active topology plus one retired resource set per possible in-flight swapchain
image; this increases reserved GPU memory relative to immediate destruction.
Shutdown still uses
`vkDeviceWaitIdle()` before flushing every remaining retirement batch. This
retirement path does not change the synchronization used by one-time staging
and upload commands. No timing or peak-memory measurements have been recorded.

The snapshot also copies the complete stencil-selection set and render mode.
This adds work linear in the number of highlighted object IDs, which is
expected to remain small. It replaces per-change heap allocation, queue locking,
and virtual dispatch in the former graphics-command path. Shutdown uses a
dedicated atomic stop request. No timing measurements have been recorded.

The unused offscreen object-preview renderer and pipeline are no longer created,
removing their fixed startup Vulkan resources. No memory measurements have been
recorded.

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

Engine-window drawing, processing, persistent-window updates, application-window
processing, and editor keyboard handling share one coarse mutex. This removes
per-window lock acquisition and also synchronizes windows that previously shared
plain state without a lock. The graphics and game threads cannot process engine
UI concurrently, and expensive panel operations such as scene or resource loading
can therefore stall UI drawing. This temporary trade-off has not been measured;
keep expensive work out of the critical section when the UI synchronization model
is refined.
