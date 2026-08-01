# Performance Notes

Record only material performance-sensitive behavior here. Label modeled costs
as expectations until they have been measured.

No CPU, GPU, frame-time, or peak-memory measurements have been recorded for
the items below.

## Point-light shadow filtering

Rasterized point-light shadows use 16-tap Poisson-disc PCF in
`shaders/library/library.glsl`, shared by the color, texture, skinned-color, and
skinned-texture pipelines.

- Each lit fragment performs 16 cubemap depth samples; these are expected to
  dominate the filter cost. Moving from 9 to 16 taps increased sample count and
  per-tap work by 78% (modeled, not measured).
- Each tap normalizes its lookup direction and applies receiver-plane depth
  correction. This costs additional arithmetic but avoids concentric
  self-shadowing on large flat receivers.
- Textured materials use the geometric normal for shadow correction so normal
  maps do not destabilize shadow depth.

Reducing the sample count is the primary quality/performance control. Keep the
loop bound and visibility divisor synchronized with the kernel size. Increasing
`texel_size` softens edges but exposes undersampling more readily. If profiling
shows arithmetic pressure, hoist loop invariants and test whether lookup
normalization can be reduced without restoring artifacts.

## Render preparation and command recording

The game thread publishes a complete immutable `RenderFrame` after each
update. Snapshot construction scales with renderable/material references,
skeleton pose data, bone attachments, particles, and highlighted object IDs.
Current and previous snapshots retain their dynamic vectors and referenced
assets until consumers release them or newer publications displace them.

The mailbox is latest-wins: a slow graphics thread drops intermediate snapshots
instead of queueing work or blocking the game thread. Graphics-owned topology
is reconciled only when renderable or skeleton membership changes; transforms,
visibility, camera state, particles, and poses reuse it.

Opaque, masked, overlay, and shadow draw lists are classified and state-sorted
only after topology changes. Blended lists are depth-sorted each graphics frame.
State ordering groups pipelines, meshes, and materials, allowing command
recording to skip redundant pipeline and vertex/index-buffer binds. Per-draw
transform and material descriptor sets are still bound for every item.

## Animation and transforms

- World transforms are composed lazily and cached. Changes dirty only the
  affected entity and its descendants; repeated reads reuse cached results.
- Skeletons have one model-space bone-buffer slot per `SkeletonID` and
  swap-chain frame, shared by all attached renderables. This avoids duplicate
  pose uploads for shared skeletons.
- Skinned shaders apply the renderable model matrix after skinning, adding
  matrix-vector work for positions and, in lit passes, normals and tangents.
- Cross-fades evaluate and retain per-bone source/target pose data while active,
  adding linear CPU work and temporary storage in bone count.
- Bone attachments compose a source skeleton's model-space pose once per frame
  and reuse it for every attachment. Work scales with bones in skeletons that
  have attachments, plus constant transform work per attachment.

Pose updates and snapshot creation stay on the game thread; the graphics thread
consumes copied poses without skeletal mutex contention.

## Deferred GPU resource retirement

Topology replacement and unused mesh/material cleanup avoid
`vkDeviceWaitIdle()`. Removed GPU allocations are released after the serial of
the last potentially referencing graphics submission completes.

This avoids device-wide runtime stalls during topology changes, but old and
replacement allocations may coexist while frames remain in flight. Uniform
buffer and descriptor-pool capacity includes the active topology plus one
retired renderable resource set per possible in-flight swap-chain image.
Shutdown still waits for the device before flushing all pending retirements.
One-time staging and upload commands retain separate queue synchronization.

## UI synchronization

Engine-window drawing and processing, persistent-window updates, application
window processing, and editor keyboard handling share one coarse mutex. This
prevents the game and graphics threads from processing engine UI concurrently.
Scene or resource loading inside the critical section can therefore stall UI
drawing; keep expensive operations outside it when refining this synchronization
model.
