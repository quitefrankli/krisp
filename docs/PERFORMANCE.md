# Performance Notes

Record only material performance-sensitive behavior here. Label modeled costs
as expectations until they have been measured.

No CPU, GPU, frame-time, or peak-memory measurements have been recorded for
the items below.

## GPU memory allocation

Engine-owned buffers and images use VMA to suballocate Vulkan device memory
instead of issuing one `vkAllocateMemory` call per resource. This is expected
to reduce driver allocation overhead and allocation-count pressure, particularly
for textures and per-frame attachments; it has not been measured. VMA may retain
partially used memory blocks, so its reserved device memory can exceed the sum
of live resource sizes. Dedicated allocations remain available when required by
the driver or selected by VMA.

An unused swap-chain-sized multisampled color image was also removed. Scene
and resolve attachments remain unchanged; VMA block retention determines how
removing that live resource affects reserved device memory.

The engine's existing buffer slot allocator and capacities remain unchanged.
Readback allocations prefer host-cached memory; uploads request sequential host
access. Flush and invalidate operations are delegated to VMA and are no-ops on
host-coherent memory. Upload submission, fence waits, and resource retirement
retain their existing synchronization costs. No frame-time or peak-memory
improvement has been measured for this migration.

## Synchronization2

Engine barriers and queue submissions use Vulkan Synchronization2. Copy and
sampled-image scopes are expressed explicitly while preserving frame ordering,
attachment reuse dependencies, capture placement, and semaphore boundaries.
The migration does not add queues, remove upload queue-idle waits, or change GPU
work counts. Timestamp queries retain their prior measurement endpoints.
No frame-time improvement has been measured from the API migration.

## Dynamic rendering

Supported graphics passes and one-time texture composition use Vulkan dynamic
rendering. This removes render-pass and framebuffer object creation/destruction,
including per-swap-chain framebuffer allocation and shadow-capacity framebuffer
replacement. Attachment images, sample counts, resolve operations, draw order,
and shadow layer counts retain their existing costs. Explicit image barriers
provide the transitions and visibility formerly expressed by render passes.
No frame-time or memory improvement has been measured for this migration.

## HDR presentation

RGBA16F scene and resolve images use twice the pixel storage and bandwidth of
the former BGRA8 path; one resolve image remains resident per swap-chain image.
Presentation adds one full-screen draw and texture sample per pixel. These costs
have not been measured.

## Point-light shadow filtering

Rasterized point-light shadows use 16-tap Poisson-disc PCF in
`shaders/library/library.glsl`, shared by all four lit material paths. Each
fragment accumulates direct illumination across up to eight snapshot lights.
Disabled lights and lights with zero intensity or black colour are skipped.
For contributing lights, both the base surface normal and the normal-map shading
normal must face the light; otherwise the contribution skips shadow sampling
and BRDF evaluation. This prevents normal maps from admitting direct light
through the back of an opaque surface. Shadow bias and receiver-plane depth
use the base surface normal, while the BRDF retains the shading normal. Each
contributing shadowed light performs 16 cube-array depth samples. Unshadowed
lights do not perform PCF.

Graphics Settings exposes a global `Shadows` switch. Disabling it preserves
direct lighting and material shading while preparing every light with shadow
slot `-1`. No shadow matrices, caster draws, or fragment shadow samples are
needed. Each swap-chain slot still initializes its bindable shadow image once;
later zero-shadow frames skip the pass, including its depth-attachment clear.
Allocated cube-array capacity is retained for re-enabling shadows. The switch
travels in the immutable render-view snapshot and does not add pipeline variants.
Its performance benefit has not been measured.

- Shadow filtering can therefore require up to 128 depth samples per fragment
  when all eight lights contribute and cast shadows. This is a modeled upper
  bound, not a measurement. Moving from 9 to 16 taps increased sample count and
  per-tap work by 78% for each shadowed contribution (modeled, not measured).
- Each tap normalizes its lookup direction and applies receiver-plane depth
  correction. This costs additional arithmetic but avoids concentric
  self-shadowing on large flat receivers. Normal maps do not perturb that
  receiver plane or its bias. No performance change from separating the
  surface and shading normals has been measured.
- Shadow command recording scales with the number of shadowed lights and the
  visible shadow-caster draw list. Each shadowed light renders eligible casters
  to six faces; there is no spatial culling by light range. This CPU and GPU
  work has not been measured.
- Each shadow cube uses six 1024-by-1024 `D32_SFLOAT` array layers, or about
  24 MiB per cube per swap-chain frame. Per-frame cube-array capacity grows to
  the next power of two and does not shrink before shutdown. For example, a
  capacity of four cubes reserves about 96 MiB per frame, subject to device
  allocation behavior. Actual peak memory and allocation costs have not been
  measured.

Reducing the sample count is the primary quality/performance control. Keep the
loop bound and visibility divisor synchronized with the kernel size. Increasing
`texel_size` softens edges but exposes undersampling more readily. If profiling
shows arithmetic pressure, hoist loop invariants and test whether lookup
normalization can be reduced without restoring artifacts.

Potential optimizations, in priority order after GPU profiling:

- Reduce the PCF sample count only if timestamp or shader-profiler evidence
  identifies shadow filtering as a material bottleneck. Revalidate shadow
  stability and edge quality whenever changing the kernel.
- Add light-volume or other spatial culling only if profiling shows the
  eight-light fragment loop or repeated shadow draws materially affect
  performance. There is no range culling today.

## Metallic-roughness shading

The static-color and skinned-color lit pipelines evaluate the glTF
metallic-roughness BRDF per fragment. Compared with their former Blinn-Phong
shading, this adds GGX distribution and correlated Smith visibility arithmetic,
including several square roots, alongside the per-contributing-light direct
lighting loop and 16-tap filter for shadowed lights. These are modeled costs
and have not been measured.

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
material-texture-free pipeline. The factor-only skinned pipeline also remains
free of material texture samples and retains its existing shared skinned-vertex
layout. Both still sample the global IBL resources when lit. Textured static and
skinned meshes use separate pipelines carrying `TEXCOORD_0`; valid tangents are
required only when normal mapping is active. This keeps factor-only vertex
bandwidth and material descriptor use unchanged at the cost of two additional
lit pipeline variants.

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

## Skybox image-based lighting

The image-based-lighting processor produces 32-by-32 diffuse irradiance, a
complete 128-by-128 roughness-prefiltered specular mip chain, and a 128-by-128
split-sum BRDF lookup texture. The current settings perform about 6.8 million
deterministic cubemap or BRDF samples.

A Debug measurement of the default settings took 3.23 seconds on the CPU:
2.44 seconds for specular prefiltering, 648 ms for the BRDF lookup, and 142 ms
for irradiance. Decoding the six default 512-by-512 JPEG faces took 41 ms by
comparison. These figures are from one development machine and are not a
cross-platform benchmark.

Krisp's default environment is therefore precomputed by an incremental Meson
target and loaded from a roughly 600 KiB versioned asset at launch. The asset
fingerprints the decoded source faces and processor settings, so stale output
is rejected. Meson regenerates it when any source face or the precompute tool
changes. Cubemaps without an associated asset retain synchronous processing as
a fallback.

The Conan engine runtime includes this precomputed asset. `spawn_cubemap()`
selects it automatically when all six faces resolve from engine defaults, so
external apps avoid repeating the CPU convolution at startup. App overrides
retain synchronous processing unless the caller supplies a matching asset.

Engine resources resolve through compiled `EnginePaths`; development builds no longer
stages, copies, or symlinks them into a runtime directory. This removes those
filesystem operations, but their performance effect has not been measured.

The three linear RGBA8 outputs use about 600 KiB of GPU image data before image
and allocator overhead: 24 KiB for irradiance, approximately 512 KiB for the
specular mip chain, and 64 KiB for the lookup texture. Generated environments
are cached by their cubemap source for the texture manager's lifetime.

Every lit fragment performs three additional logical lookups: diffuse cubemap,
roughness-selected specular cubemap, and BRDF LUT. Trilinear specular sampling
may fetch from two mip levels. Reflection and Fresnel arithmetic are also added;
these costs have not been measured. Unlit pipelines do not declare or sample
the environment resources.

Replacing or removing an initialized environment waits for device idle before
updating the shared global descriptor sets. The default startup path does not
incur this replacement stall. For dynamic environments, prefer GPU/background
environment convolution before reducing output resolution or sample counts;
validate any quality reduction against rough metals and broad highlights.

PNG/JPEG decoding and MikkTSpace tangent generation are model-load costs.
MikkTSpace may split vertices at tangent discontinuities, increasing vertex and
index storage for affected normal-mapped meshes. Decoded PNG/JPEG normal maps
now build a complete RGBA8 mip chain on the CPU during loading. Area filtering
and vector renormalization add work proportional to the image's texel count;
this cost has not been benchmarked. The mip payload is retained on the CPU and
uploaded to the GPU. Texel payload is the sum of `4 * mip_width * mip_height`
over supplied levels, about one third more than the base for large square
textures, and can approach twice the base for long, thin images. Other decoded
texture semantics remain single-level RGBA8, approximately
`width * height * 4` bytes before allocator overhead. Missing mip chains can
alias during minification. Supplied DDS chains are preserved without runtime
generation.

The `optimize_resource_loader` Meson option defaults to `true`. It compiles
`resource_loader.cpp` and its included mesh, animation, and material
implementation files with optimization level 3, independent of the rest of the
build. Debug symbols and the build's assertion policy remain unchanged. Set
`-Doptimize_resource_loader=false` (for example, with
`meson configure build/debug -Doptimize_resource_loader=false`) to step through
the loader without this optimization. Calls into code compiled in other units
are unaffected, and no speedup has been measured.

Standalone and `MSFT_texture_dds` DXT5/BC3 textures stay block-compressed during
upload and sampling. Each mip uses roughly one byte per pixel, rounded to 4-by-4
blocks; a complete chain adds about one third over the base level. This reduces
image memory and sampling bandwidth relative to RGBA8, while authored mip levels
also reduce minification aliasing. Trilinear filtering may read two mip levels
per sample; nearest-mip filtering reads one and can show transitions. These
trade-offs have not been measured in Krisp.

Decoded image data and GPU images are shared across material users when their
image and semantic permit the same colour-space interpretation. Sampler objects
are shared by address and mip-filter state, and each descriptor records the
requested sampler. The shared neutral textures are engine-wide. This avoids
duplicate decoding, uploads, and image memory, while each distinct
sampled-material binding still consumes its descriptor records. Texture upload,
sampling, descriptor pressure, and tangent-generation costs have not been
measured.

## Alpha, double-sided, and emissive materials

Masked materials add an alpha comparison and fragment discard to their colour
and shadow passes. A base-colour texture used for masking must also be sampled
in the shadow pass, increasing shadow bandwidth relative to fully opaque
materials. Discarded fragments can reduce later shading work, but may also
reduce early-depth efficiency; neither effect has been measured.

Double-sided materials disable face culling and use separate cached pipeline
variants. They can rasterize roughly twice as many fragments for thin closed
geometry and increase overdraw for layered cards. Masked, blended, and
double-sided state combinations also increase pipeline-cache cardinality.

An emissive texture adds one sRGB texture sample per fragment. Factor-only
emission adds arithmetic but no texture sample. These costs have not been
measured.

Blended renderables are depth-sorted every graphics frame and cannot use opaque
early-depth rejection in the same way as opaque geometry. Their CPU sorting
cost grows with the number of blended draws, while overlapping transparent
layers increase fragment overdraw. Sorting is per renderable rather than per
triangle; improving difficult transparency cases may require additional CPU
work, draw splitting, or a different transparency technique.

## Render preparation and command recording

The game thread publishes a complete immutable `RenderFrame` after each
update. Follow cameras update after physics and application tick callbacks, so
the published camera and its target use the same completed simulation pose.
Updating follow before physics causes a one-tick relative displacement during
controller movement and jump takeoff. Snapshot construction scales with renderable/material references,
skeleton pose data, bone attachments, particles, and highlighted object IDs.
Current and previous snapshots retain their dynamic vectors and referenced
assets until consumers release them or newer publications displace them.

The mailbox is latest-wins: a slow graphics thread drops intermediate snapshots
instead of queueing work or blocking the game thread. Graphics-owned topology
is reconciled only when renderable or skeleton membership changes; transforms,
visibility, camera state, particles, and poses reuse it.

Swap-chain frame resources remain per image. Graphics acquires an image before
choosing its `GraphicsEngineFrame` and recording commands. Rotating acquisition
semaphore slots wait for their last submission fence before reuse; the acquired
image's frame fence is also waited before updating its resources. Recording
after acquisition can reduce CPU and window-system overlap, but this effect has
not been measured. Draw submission does not wait for the queue to become idle.

## Frame-pacing investigation

A diagnostic run reproduced visible motion stutter while game publication and
graphics draw rates both averaged near 60 TPS/FPS. The independently paced loops
and latest-wins mailbox showed repeated snapshots followed by skipped game-frame
numbers as their phases drifted; clean discontinuities did not coincide with UI
mutex or present-call stalls, although separate UI-heavy long frames occurred.
Translation-only interpolation across one tick did not make motion consistently
smooth, so repeats and skips are a confirmed contributor but do not yet explain
all perceived stutter.

Deterministic screen recording deliberately changes this scheduling policy for
the duration of a capture. The game publishes one fixed-step frame and waits
for graphics to copy it, so render and encoder stalls slow wall-clock capture
without changing video time. GPU readback still waits for the submitted frame
fence. The encoder runs on its own thread with at most two queued BGRA frames;
when that queue fills, capture applies backpressure instead of allowing memory
use to grow without bound. At 1920x1080 the two queued source frames occupy
about 16 MiB before encoder and staging allocations.

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
- Target-skeleton model imports validate and remap compatible skinned meshes at
  load time so character accessories reuse that slot instead of allocating and
  synchronizing a redundant skeleton.
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
PBR base-colour overlays use this cached output through the existing texture
slot, adding no per-frame texture samples. Appending an overlay creates a new
immutable recipe and regenerates the complete flattened stack once; it does
not incrementally draw into the previous output. Retained render snapshots may
briefly keep both outputs alive until submission-safe retirement completes.
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

## Static triangle-mesh collision

Indexed triangle collision meshes are cooked into Jolt MeshShape acceleration
trees when bodies are created. Cooking is synchronous and repeated when a body
is reconstructed; it is not per-frame work. Each body currently retains its
source vertex/index vectors for scene serialization in addition to Jolt's cooked
shape. There is no cross-body cooked-shape cache in this API.

Mesh queries use Jolt's tree to prune distant triangles. This is a structural
property, not a measured timing claim. Prefer simplified collision geometry and
manageable static sections; use primitive shapes where they describe the surface
adequately. No cooking, query-time or memory benchmark is claimed for this change.

## Virtual capsule controllers

Virtual capsule controllers advance at the existing 60 Hz physics step, with
the same maximum of four accumulated steps. Each controller performs Jolt
collision queries through ExtendedUpdate, including configured stair and short
ground-adhesion checks. Controllers update sequentially before rigid-body
simulation; no timing or capacity benchmark is claimed.

Each controller has an inner kinematic capsule for scene queries and physical
presence. Its lifetime belongs to the controller and it is not a second
application-managed rigid body. Reuse live controllers rather than reconstructing
them per frame. Application-requested planar velocity does not reset accumulated
vertical velocity; gravity and vertical launches advance on fixed steps.

## Responsive startup loading

The GLFW window remains hidden during synchronous GameEngine and Vulkan-backed
GraphicsEngine construction; the desktop-managed window first appears when
`GameEngine::run()` starts. The loading overlay is presented before the startup
scene callback and CPU asset preparation begin. Registered-source parsing and
decode run on a CPU worker, while live ECS adoption and provenance updates stay
on the game thread. Event polling continues at 10 ms intervals during worker
waits, with an additional checkpoint between model mesh-node groups. Arbitrary
consumer CPU work in the scene callback or `on_begin()` runs synchronously on the
game thread without event pumping and should remain short.

The graphics thread prepares the startup frame incrementally, with a limit of
eight renderables or four milliseconds of cooperative renderable-upload work
per draw iteration. Environment-lighting setup and skeleton-buffer preparation
run before this budgeted loop. The time limit is checked between renderables;
one Vulkan upload is not preemptible and can exceed it. This cooperative budget
is not a hard real-time guarantee. The loading overlay remains until the
graphics thread acknowledges presentation of the completed startup frame. These
limits bound work between cooperative checkpoints, not total startup time. No
startup frame-time, loading-time, or upload-size measurements have been
recorded.

CPU workers touch source-owned decode caches. The game thread owns live ECS,
object, and provenance mutation; the graphics thread owns scene Vulkan
realization and upload. A close or Escape request waits for an in-flight CPU
preparation to finish while event polling continues, so worker-owned inputs
remain alive until the task completes. Atomic `GenericID` allocation permits
mesh and material construction on a worker while rendering allocates other
material IDs.

## Registered asset decoding

`AssetRegistry` retains decoded CPU data by registered source for the engine
session. The first selected part from a glTF/GLB document parses and retains the
whole TinyGLTF document, including its buffers and dependent decoded images.
Selectors therefore reuse parsing and image decode, but do not provide
per-primitive reads or selective dependency-image retention. A document with
large unused images can retain more CPU memory than the requested exports
alone require.

Decoded meshes and material payloads are immutable. Existing `CountableSystem`
stores can adopt them through shared ownership and reuse IDs, so adoption does
not copy CPU geometry or material data. Texture handles remain distinct by
semantic because sRGB and linear interpretations need separate GPU resources;
the registered source keeps the decoded image data shared. Provenance records
share one immutable registration descriptor per source. Lazy registration
defers decoding until acquisition, while eager registration shifts that work to
registration and retains the same decoded data. Successful registration
commits only new source/name map nodes; re-registering an identical source does
not copy the registry maps or reread its file. These are structural cost
expectations; no decode-memory or isolated GPU-upload measurements have been
recorded. The asset-loading suite records end-to-end CPU wall-clock timings for
registration, instantiation, scene setup, and presentation readiness, as
described below.

## Performance benchmark mode

The benchmark harness runs independent simulation and graphics loops using
explicit runtime overrides. Simulation targets 60 Hz with a fixed 1/60-second
step. The throughput mode removes engine rendering sleep and requests IMMEDIATE
presentation, while the pacing mode targets 60 Hz and prefers MAILBOX. Unsupported
presentation preferences fall back to available modes and are recorded. Driver
and compositor throttling can still limit throughput; submission rate is not
isolated GPU capacity or measured display scanout cadence.

The `benchmarks -- performance` mode defaults to the `standard` suite. It runs
`moving-transforms-500` and `point-shadows-500` in throughput mode, plus the shadowed Sponza atrium in
both throughput and paced modes. Each case/mode combination has two fresh
workers; two seconds of warm-up and ten seconds of measurement per worker give
eight launches and 96 seconds of configured timed work. Sponza startup makes
the end-to-end duration hardware-dependent, and first-time asset preparation
is outside the configured timed-work total. The shared cache can be prepared
manually with `benchmarks -- performance prepare-assets`; `run` prepares it
automatically when missing.

The optional standard `textured-environment-atrium-shadows-environment`
workload combines the one-light shadowed atrium with the default engine
skybox and precomputed environment lighting. It adds a skybox renderable and
diffuse/specular image-based lighting evaluation to the scene. Environment
resources are loaded before steady-state measurement; the environment asset
fingerprint and enabled state participate in baseline identity. Existing
workloads retain their lighting setup. This adds measurement coverage without
a measured performance claim; interactive runs produce no benchmark samples.

The default shadowed atrium workload renders the existing fixed point light's
1024-by-1024 shadow cubemap and samples it with the current 16-tap PCF filter.
The unshadowed `textured-environment-atrium` workload remains selectable for a
same-view comparison under matching Release settings; corridor and
material-closeup workloads remain unshadowed. Prior Sponza GPU observations
used the unshadowed atrium and therefore excluded point-light shadow-map
rendering and shadow filtering. The new workload includes those costs in the
full rasterization workload. It supplies coverage for future measurement;
there are no measured benefit or regression claims from it yet. Shadowed and
unshadowed runs are distinct workload baselines, with actual shadow-light
count recorded as part of compatibility.

`--suite smoke` runs a diagnostic case, a Flight Helmet case and a fixed Sponza
atrium view in both modes as a short Release lifecycle check; the shadowed
atrium can be selected explicitly without changing smoke's default workload
set. Smoke's short measurements are not performance evidence. All performance
benchmark suites require optimized Release builds with assertions disabled.
A separate
`--suite asset-loading` measures the Flight Helmet and Sponza atrium from fresh
worker processes through application-observed first presentation. Loading
results report stage timings and end-to-end readiness; they do not replace the
standard suite's warmed rendering metrics. Asset loading continues to use the
existing unshadowed atrium workload. See [BENCHMARKS.md](benchmarks/BENCHMARKS.md) for
commands, protocol overrides, saved evidence and explicit baseline selection.

The benchmark's `engine-pbr-opaque-bc3-normal-mips-v1` asset profile derives
BC3/DXT5 DDS textures from the pinned source PNGs during preparation. Normal
maps receive full chains filtered as vectors and renormalized before BC3
compression; other images remain base-level only. This adds offline filtering
and compression work and increases runtime normal-map payloads. This is a
lossy conversion, including for normal maps; source images remain available in
the cache, while runtime glTF references the DDS files without PNG fallbacks.
Profile, converter version, and exact runtime-file hashes identify the prepared
inputs, so PNG-profile results do not compare as the same asset set. The
conversion is an approved benchmark preparation choice, not a measured loading
improvement. See [BENCHMARK_ASSETS.md](benchmarks/BENCHMARK_ASSETS.md) for cache
migration and offline preparation details.

The regenerated pinned cache contains 28 normal maps with complete chains.
Only those DDS outputs and the two glTF image references differ from the prior
recipe; the other runtime-file hashes are unchanged. Extra compressed mip
payload is 5,942,064 bytes for the smaller asset set and 128,625,936 bytes for
the larger environment, totaling 134,568,000 bytes (about 128.3 MiB). These
are file-payload counts, not measured GPU allocations or performance results.

Benchmark workers explicitly reserve 128 MiB of vertex-buffer capacity and
64 MiB of index-buffer capacity for every scenario. These fixed per-run limits
accommodate the prepared large-scene workload without reducing its geometry.
They increase reserved GPU memory relative to ordinary engine defaults; the
engine exposes generic startup capacity overrides and retains its existing
limits for consumers that do not request them. Actual imported counts and
memory estimates remain part of the prepared-asset audit.

Optional runtime observation allocates two fixed-capacity producer buffers and
copies fixed-size records on game and graphics threads. The consumer drains and
serializes records outside measured frame work. Overflow is observable and
invalidates benchmark results; no unbounded queue or per-frame file output is
introduced on the frame producers. Enabled collection adds monotonic clock
reads, buffer copies and GPU timestamp writes; its overhead has not been
quantified. Ordinary consumers retain their scheduling and do not allocate
observation/query resources unless they opt in.

CPU observations separate simulation work (including snapshot publication),
graphics loop duration, fence waits, image acquisition, submit and present calls,
and pacing sleep. Enclosing graphics intervals include their child calls;
reports subtract their interval union for active CPU work rather than summing
nested durations. CPU elapsed work is not CPU utilisation. Snapshot identifiers
and publication times distinguish simulation progress from repeated or skipped
snapshots without replacing the latest-wins mailbox with lockstep rendering.

GPU samples bracket the complete graphics command span with timestamp queries,
including shadow and presentation rendering and any device-side stalls in that
span. Query readback follows the existing frame fence; it adds no per-frame
completion wait. Query storage and pending submission identity belong to the
in-flight frame. Unsupported timestamps and lost results remain explicit.
For the standard and smoke suites, startup, loading, GPU upload, warm-up, final
collection and shutdown are excluded from measured distributions.

Benchmark active-work aggregation merges child timing intervals once and indexes
cumulative union coverage. Each parent interval uses two binary searches rather
than scanning every child interval, preserving clipped-overlap and nested-wait
semantics with O(C log C + P log C) work for C child and P parent intervals.
The previous quadratic scan caused roughly 499 seconds of between-worker gaps
in the eight-launch run `run-20261021T094736Z-4fe3b367` (615 seconds overall).
Replaying its first moving-transforms summary took 165.1 seconds before this
change; after indexing it took 1.48 seconds including trace reading. All eight
saved summaries replayed in 5.92 seconds and exactly matched their original
metrics. These replay timings are separate from an end-to-end engine run.
This is coordinator post-processing cost outside measured engine work.

Validation-enabled results remain labelled and cannot be compared with a
validation-disabled baseline.

Rendering observation protocol 3 adds CPU distributions for `UniformUpload`
(global/object/skeleton data preparation and writes), `CommandRecording`
(command-buffer reset, recording and end), `SnapshotConstruction` (build and
publication), and `ResourceUpload` (loading preparation or topology
reconciliation, including resource creation and uploads). Resource-upload samples
are event-driven and may be absent in a steady scene. They are not isolated DMA
transfer times. These spans are useful work nested within existing CPU intervals;
they are not subtracted from active-work totals or added to their parent times.

GPU observation keeps the existing whole-command-span `Gpu` measurement and adds
`GpuShadow`, `GpuMain` (rasterization including particles), and `GpuUi` (GUI).
Pass spans use bottom-of-pipe timestamps immediately before and after their
renderer commands, outside render passes. Query results retain submission IDs
and are read after the existing frame fence, without a query wait flag or an
extra fence wait. Queue boundary spans can reflect pipelining and device stalls;
they are not isolated shader costs and do not partition all work in the total.
For the shadowed Sponza workload, `GpuShadow` includes the fixed light's shadow
cube rendering and `GpuMain` includes atrium rasterization with shadow
filtering. The unshadowed atrium does not perform those point-light shadow
operations.
The observer creates eight timestamp queries per in-flight frame instead of two,
and publishes four GPU records per observed submission. No timestamps or CPU
phase timing records are generated without an active observer. Added observation
changes instrumentation overhead, so rendering instrumentation version 2 and
protocol 3 deliberately invalidate earlier rendering baselines. Use Release
measurements to rank uniform-upload, command-recording and GPU-pass costs before
optimizing them; instrumentation correctness checks provide no performance
evidence. Every performance benchmark suite, including smoke, must run in
optimized Release with assertions disabled.


The asset-loading suite reports monotonic CPU wall-clock durations for engine
construction, asset registration, model instantiation, scene setup, presentation
readiness, asset-to-ready, and engine-to-ready. Asset-to-ready sums registration
through presentation readiness; engine-to-ready begins before construction and
includes the pre-asset-loading frame. Presentation readiness covers scene-end
through immutable publication, incremental GPU upload, first presentation, and
game-thread observation. This boundary measures end-to-end startup; it does not
isolate GPU upload or physical scanout. Asset preparation and checksum
verification occur before timing. Each worker is a fresh process, but OS file
cache state is uncontrolled, so the results do not represent a cold-cache
protocol. No GPU queries or frame telemetry run in loading workers.

Repeated worker summaries, raw records and effective settings support
same-machine comparisons. Variation and compatibility limits remain visible;
the tool produces neither an overall score nor automatic baseline promotion.
No benchmark throughput, frame-pacing improvement, loading-stage attribution
beyond the named boundaries, or instrumentation-overhead measurement is claimed
here. See [RUNTIME_OBSERVATION.md](RUNTIME_OBSERVATION.md) for the engine-owned
observation boundaries and threading contract.

Every benchmark run, including smoke, requires an optimized Release build with
assertions disabled. Smoke is a short lifecycle check and does not provide
sufficient performance evidence.

Benchmark workers enable the FPS/TPS performance overlay for loading where
available, warm-up, and measurement. Overlay updates and drawing remain inside
the measured CPU and GPU work; reported timings do not subtract their cost.
Displayed FPS/TPS are live diagnostics, while benchmark reports use the raw
observation records. The overlay adds work to each run, and its overhead has
not been measured.

Asset-loading instantiation attribution is opt-in and adds monotonic clock
reads per observed stage plus collection overhead. Its exclusive spans separate
dependency reads, image decoding, document processing, residual model/mesh
conversion, material construction, and ECS adoption; unattributed wall-clock
time remains explicit. Nested spans do not double-count their child stages.
The loading protocol records instrumentation version 2 so instrumented results
are not compared with earlier uninstrumented loading runs. Attribution
correctness checks are diagnostic evidence, not performance measurements.

A same-machine Release comparison on the RTX 2070 used the normal graphics
session, 1280x720, 4x MSAA, validation disabled, two repetitions, 2 seconds of
warm-up, and 10 seconds of measurement. Unshadowed atrium run
`run-20261021T110342Z-da2398ca` achieved 279.6–281.4 uncapped presents/s with
3.325–3.326 ms median GPU work. Shadowed atrium run
`run-20261021T110450Z-146b7bfc` achieved 99.6–100.1 presents/s with 9.812–9.853 ms
median GPU work; its shadow span was 6.135–6.162 ms and main span 3.334–3.346 ms.
CPU command recording increased from 0.337–0.342 ms to 0.617–0.641 ms.
Both variants averaged approximately 60 presents/s when paced, but paced p99
present-call intervals reached about 28 ms in both and simulation deadline misses
were observed. These runs establish substantial shadow-generation cost for this
one-light scene; they do not establish consistent frame pacing or an eight-light
worst case. An earlier isolated-display comparison was presentation-limited
(about 29 ms per present call) and is not used for these throughput figures.

Optional `textured-environment-atrium-shadows-4-lights` and
`textured-environment-atrium-shadows-8-lights` workloads extend the one-light
control without increasing the default standard schedule. All modes use a fixed
nested prefix of eight distinct positions, with the original light first. The
pinned total intensity parameter is divided equally among active lights; this
controls aggregate intensity, not perceived scene brightness. Actual positions,
colors, intensity, enabled state and shadow flags are recorded in scenario
metadata and baseline compatibility alongside the requested and observed counts.

A normal-display Release comparison used the same RTX 2070, 1280x720, 4x MSAA,
1024-square shadow cubemap faces, existing 16-tap PCF, validation disabled, two
repetitions, 2 seconds of warm-up and 10 seconds of measurement per launch.
All twelve launches completed with no lost observation records. Throughput
results below show repetition ranges for rates and the approximate mean of the
two per-repetition medians for GPU spans; the spans are not additive partitions.

| Shadow-casting lights | Uncapped presents/s | Total GPU ms | Shadow GPU ms | Main GPU ms |
| --- | --- | --- | --- | --- |
| 1 | 100.2–100.4 | 9.77 | 6.15 | 3.33 |
| 4 | 32.9–33.5 | 29.07 | 22.96 | 6.06 |
| 8 | 16.4–16.8 | 58.46 | 47.24 | 11.10 |

The corresponding runs are `run-20261021T111211Z-837beefd` (one light),
`run-20261021T111318Z-5deb16c3` (four), and
`run-20261021T111425Z-595d5c0b` (eight). Shadow generation is the dominant measured
scaling cost in these scenes, while the main pass also grows with light count.
The paced four- and eight-light variants achieved only about 30 and 15–16
presents/s, respectively, with substantial frame-time tails and simulation
deadline misses. They do not meet the 60 Hz target. These measurements justify
profiling and reducing shadow-generation work before prioritizing small uniform
upload savings; they do not attribute the cost specifically to geometry-shader
expansion versus absent spatial culling or establish the effect of any proposed
optimization.

Source inspection and published comparisons are recorded in
[SPONZA_SHADOW_COMPARISONS.md](benchmarks/research/SPONZA_SHADOW_COMPARISONS.md).
The eligible prepared scene contains 3,738,878 opaque caster triangles in 401
draws per light (four blended primitives are excluded). The six-face geometry
shader therefore emits about 179.5 million triangle copies at eight lights before
clipping. The nearly linear measured generation time is plausible for this
unculled, uncached path, but does not establish a normal expected cost for eight
point lights in an optimized engine. Published legacy-Sponza measurements have
different geometry, GPU, light type or shadow update policy; no matched Unity or
UE5 number is claimed. The main-pass PCF loop is separate from shadow generation.
Controlled resolution, face-culling and cached/static experiments should precede
any quality reduction or renderer rewrite.

One-light Release attribution probes subsequently reproduced about 6.05–6.23 ms
of shadow generation. Changing only face resolution from 1024² to 256² left
about 5.95–6.06 ms despite sixteen times fewer texels. Optimizing shadow SPIR-V
with `glslc -O` gave 6.09 ms; a deliberately incorrect projective-depth diagnostic
gave 6.07 ms. Conservative per-triangle cubemap-face rejection before geometry
emission reduced the 1024² generation span to 2.84 ms and increased uncapped
presents from roughly 100/s to 150/s. This rejects triangles that hardware would
clip away, while retaining the six-face input-transform work. The measurements
point to avoidable geometry output/clipping as a major cost and do not yet
separate the remaining geometry-pipeline costs. Those diagnostic probes restored
original SPIR-V and the 1024² configuration and rebuilt Release. Setup, controls,
limitations and raw artifact
paths are documented in the linked research note.
A further six-invocation/one-face-per-invocation geometry-shader prototype was
slower (6.92 ms without rejection, 3.28 ms with rejection) than the original
loop plus rejection (2.84 ms). Its original-path controls were stable at
6.17/6.15 ms. This supports implementing and validating conservative rejection
first, rather than assuming a different geometry invocation layout will help.
All 26 diagnostic Release launches completed and restored the original shaders
and 1024² Release configuration before production integration.

Production shadow geometry shaders now reject a triangle for a cubemap face
only if all three clip-space vertices lie strictly outside a common Vulkan
clip plane. One shared predicate covers all six opaque, masked and skinned
shader variants. Touching and crossing triangles remain for hardware clipping;
retained triangles reuse the calculated positions and preserve their varyings.
This still processes every input triangle and calculates six sets of transforms.
Shadow resolution, radial depth encoding, update frequency and PCF are unchanged.

A shader-binary A/B comparison on the same normal-display RTX 2070 used Release,
1280x720, 4x main-pass MSAA, one light with 1024² faces, two repetitions per mode,
2 seconds of warm-up and 10 seconds of measurement, with validation disabled.
The original geometry shaders were compiled from HEAD for the control; the
remaining shader stages and executable were identical. All eight standard
workload launches completed with no lost observation records. GPU columns are
means of the two per-repetition medians, not additive partitions.

| One-light atrium throughput | Before | After |
| --- | --- | --- |
| Uncapped presents/s | 99.0–100.5 | 138.2–140.2 |
| Shadow GPU ms | 6.137 | 3.367 |
| Total GPU ms | 9.823 | 6.963 |
| Main GPU ms | 3.382 | 3.372 |

This production implementation reduced the measured generation span by about
45% and increased uncapped throughput by about 40%. Both paced variants averaged
60 presents/s; this does not establish improved frame-time tails. The production
shared-predicate result is slower than the earlier inline rejection prototype's
2.84 ms, so that prototype number is not claimed for the integrated change.
Before/after manifests and raw data are respectively
`run-20261021T115104Z-0d65ec67` and `run-20261021T115212Z-a6c87253` under
`build/shadow-face-rejection-verification/20261021T225104-1c0068/data/krisp-engine/stress-test/`. This recorded run predates the result-directory rename; new runs use `krisp-engine/benchmarks/performance/`.
The parent directory retains shader hashes, compiled variants, image captures
and a combined `results.json` identifying the binary A/B controls.

Isolated-display captures of the atrium, corridor and material-closeup views
were pixel-identical before/after outside the changing FPS/TPS overlay. These
captures are correctness evidence only. A temporary Vulkan compute harness
executed the actual production predicate on the RTX 2070 and llvmpipe: 5,016
cases passed, including all six plane boundaries, crossings and mixed/negative
homogeneous w, with 5,000 random triangles checked against independent
double-precision polygon clipping. Harness and results are under
`build/clip-rejection-validation/`. All 60 built shaders passed SPIR-V validation
and all six existing Meson test suites passed. Runtime image coverage here is
the static opaque scene; masked and skinned variants were compiled and share the
verified predicate, but were not independently image-compared.

An additional Release validation-layer check and its original-shader control
both completed and reported only the same existing instance-creation error,
`VUID-VkInstanceCreateInfo-pNext-04926`: a debug-messenger structure appears in
the instance pNext chain without enabling `VK_EXT_debug_utils`. Externally
enabling the validation layer exposed this unchanged setup issue; no additional
VUID was reported by the optimized shader run. These diagnostic runs are not
used for performance figures. Logs are
`build/shadow-face-rejection-validation-layer{,-baseline}.log`.

The grainy benchmark image was subsequently traced primarily to normal-map
minification without mipmaps. Original PNG normal maps remain grainy, whereas
adding DDS normal-map mip chains substantially reduces the artifact. This is
a visual-correctness finding; the diagnostic captures provide no performance
claim. The engine already supports loading and sampling multi-level BC3 DDS.
The preparation recipe used for those captures emitted and validated only a
base level; the normal-mip profile now generates full normal-map chains.
The diagnostic normal-map mip chains add 128,625,936 compressed bytes (about
123 MiB, or 33% over those base levels), so adopting them changes texture memory
and sampling work and requires new pinned asset metadata and compatible
benchmark baselines. Production assets and shaders were restored; details and
controlled evidence are in
[SPONZA_GRAIN_INVESTIGATION.md](benchmarks/research/SPONZA_GRAIN_INVESTIGATION.md).
