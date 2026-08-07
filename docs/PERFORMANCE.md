# Performance Notes

Record only material performance-sensitive behavior here. Label modeled costs
as expectations until they have been measured.

No CPU, GPU, frame-time, or peak-memory measurements have been recorded for
the items below.

## HDR presentation

RGBA16F scene and resolve images use twice the pixel storage and bandwidth of
the former BGRA8 path; one resolve image remains resident per swap-chain image.
Presentation adds one full-screen draw and texture sample per pixel. These costs
have not been measured.

## Point-light shadow filtering

Rasterized point-light shadows use 16-tap Poisson-disc PCF in
`shaders/library/library.glsl`, shared by the static-color and skinned-color lit
pipelines.

- Each lit fragment performs 16 cubemap depth samples; these are expected to
  dominate the filter cost. Moving from 9 to 16 taps increased sample count and
  per-tap work by 78% (modeled, not measured).
- Each tap normalizes its lookup direction and applies receiver-plane depth
  correction. This costs additional arithmetic but avoids concentric
  self-shadowing on large flat receivers.

Reducing the sample count is the primary quality/performance control. Keep the
loop bound and visibility divisor synchronized with the kernel size. Increasing
`texel_size` softens edges but exposes undersampling more readily. If profiling
shows arithmetic pressure, hoist loop invariants and test whether lookup
normalization can be reduced without restoring artifacts.

Potential optimizations, in priority order after GPU profiling:

- Skip shadow-map sampling when `N dot L <= 0` or the active light intensity is
  zero. Direct illumination is already zero in those cases, so the current 16
  cubemap samples cannot affect the result. The branch should be coherent over
  broad unlit surface regions, but its practical benefit has not been measured.
- Reduce the PCF sample count only if timestamp or shader-profiler evidence
  identifies shadow filtering as a material bottleneck. Revalidate shadow
  stability and edge quality whenever changing the kernel.

## Metallic-roughness shading

The static-color and skinned-color lit pipelines evaluate the glTF
metallic-roughness BRDF per fragment. Compared with their former Blinn-Phong
shading, this adds GGX distribution and correlated Smith visibility arithmetic,
including several square roots, while retaining the existing 16-tap point-light
shadow filter. This is a modeled cost and has not been measured.

Point-light radiance now uses inverse-square attenuation. Static and skinned
lit vertices also compute an inverse-transpose normal matrix so non-uniformly
scaled models shade correctly. The static transform could be precomputed on the
CPU if profiling shows the per-vertex inverse to be material; the skinned paths
require a normal transform derived from each vertex's blended skin matrix.

Potential normal-transform optimizations:

- Precompute and upload the static normal matrix once per renderable transform
  instead of evaluating `inverse(mat3(model))` for every static vertex.
- Retain the per-vertex inverse-transpose for skinned meshes unless a cheaper
  formulation is proven correct for blended bones and non-uniform scaling.
  Performance alone is not sufficient justification for approximating this
  transform.

Factor-only static meshes retain their compact colour-vertex layout and
texture-free pipeline. The factor-only skinned pipeline also remains
texture-free and retains its existing shared skinned-vertex layout. Textured
static and skinned meshes use separate pipelines carrying `TEXCOORD_0`; valid
tangents are required only when normal mapping is active. This keeps factor-only
vertex bandwidth and descriptor use unchanged at the cost of two additional lit
pipeline variants.

Every textured material descriptor contains valid base-colour,
metallic-roughness, and normal images. Missing slots bind shared 1-by-1 neutral
textures, while material flags skip the corresponding shader samples. This
avoids pipeline variants for every optional-map combination; a present map adds
one texture sample per lit fragment, and a normal map also adds tangent-basis
arithmetic. These costs have not been measured.

Lit and unlit shading are separate cached pipeline variants for each supported
vertex layout, including post-stencil selection variants. This can increase
pipeline count when both policies are used, but does not create variants for
individual material texture-slot combinations. Unlit fragments skip the PBR,
light, normal-map, and shadow-filter work; textured unlit fragments sample only
the base-colour texture when present. Unlit renderables are also omitted from
shadow command recording. These savings and pipeline-cache costs have not been
measured.

PNG/JPEG decoding and MikkTSpace tangent generation are model-load costs.
MikkTSpace may split vertices at tangent discontinuities, increasing vertex and
index storage for affected normal-mapped meshes. Decoded images are uploaded as
single-mip RGBA8 textures, so expected GPU storage is
approximately `width * height * 4` bytes before allocator overhead. Missing mip
chains can alias during minification. DXT5/BC3 standalone textures use roughly
one byte per pixel per mip level and may retain a complete authored mip chain.

Decoded image data and GPU images are shared across material users when their
image and semantic permit the same colour-space interpretation. Sampler objects
are shared by addressing mode and each descriptor records the requested sampler.
The shared neutral textures are engine-wide. This avoids duplicate decoding,
uploads, and image memory, while each distinct sampled-material binding still
consumes its descriptor records. Texture upload, sampling, descriptor pressure,
and tangent-generation costs have not been measured.

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

## Runtime texture composition

Immutable base-colour compositions are generated once on the GPU when their
material first becomes live. Work scales with output pixel count multiplied by
layer count because each layer is one full-output draw. All newly introduced
compositions are generated before the first scene pass that can sample them, so
introducing many compositions together may cause a one-frame GPU workload spike.
Recipes are capped at 64 total layers, including the bottom/base texture. The
descriptor pool has a fixed engine-wide allowance of 128 compositor layers
rather than reserving the recipe maximum for every possible renderable. Layer
descriptor sets currently remain allocated for the lifetime of each cached
composition, so this allowance limits the combined layers of live compositions.

Each cached output is an uncompressed, single-mip RGBA8 image. Expected image
memory is therefore approximately `width * height * 4` bytes before allocator
overhead; a 1024-by-1024 composition uses about 4 MiB. Outputs are shared by
renderables that share a composition material and are retired after their final
referencing graphics submission completes.

Composed textures intentionally have no mipmaps. Minification may therefore
alias or shimmer; composition output resolution and layer count are the primary
quality, memory, and generation-cost controls until measurements justify a
different design.

## Jolt physics

Physics advances at 60 Hz and retains at most four fixed steps of accumulated
frame time. This keeps simulation results stable while bounding recovery work
after a slow frame. Jolt jobs run on the available worker threads and each world
owns a 10 MiB temporary allocator. Body, pair, and contact capacities are fixed
at 65,536, 65,536, and 10,240 respectively.

Continuous collision detection is opt-in and is enabled for billiard balls;
ordinary bodies use cheaper discrete motion. External transform edits teleport
bodies into Jolt before stepping. Shape changes should be batched because they
require body reconstruction.
