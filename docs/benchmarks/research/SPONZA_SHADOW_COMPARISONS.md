# Published Sponza shadow timings

## Scope

These are source-reported measurements, not a matched benchmark against Krisp. Krisp’s Intel New Sponza contains 3,747,018 triangles; the shadow draw list excludes four blended primitives and submits 3,738,878 caster triangles across 401 draws per light. With eight lights, that is 29,911,024 input-triangle submissions. The geometry shader emits 179,466,144 triangle copies (538,398,432 vertices) before clipping; these counts describe shader output, not triangles actually rasterized. The audited path has no spatial culling or shadow-map caching. Each point-light cubemap face is 1024² and the shadow pass uses 1× MSAA; the main 1280×720 pass uses 4× MSAA on an RTX 2070. The sources below use older Crytek Sponza or do not report enough scene detail to establish a scene match. A [published Forward+ project](https://github.com/WindyDarian/Vulkan-Forward-Plus-Renderer) reports its Sponza at 262,267 triangles, illustrating the large scale difference from Intel New Sponza; those numbers should not be treated as interchangeable.

## Useful measurements

| Source and setup | Reported GPU timing | What is and is not comparable |
| --- | --- | --- |
| [MJP, “Bindless Texturing for Deferred Rendering and Decals”](https://therealmjp.github.io/posts/bindless-texturing-for-deferred-rendering-and-decals/) — author reports timestamp-query GPU timings at 1920×1080 on GTX 970, default CryTek Sponza camera; 20 hand-placed spotlights with 1024² shadow maps each, plus a sun with four 2048² cascades. | Shadow-map rendering: **1.04 ms** for the spotlights in aggregate and **1.30 ms** for the sun cascades. Total deferred frame: 7.40 ms without MSAA; 9.86 ms with 4× MSAA. Timings are averages over 64 frames, with the accompanying maxima also reported. | Best source for an aggregate many-light shadow-pass figure. It does not state triangle count or shadow caster culling behavior. The measured shading used a heavy 7×7 PCF kernel, but that affects shadow lookup/shading more directly than the reported shadow-map rendering stages. Its 20 spotlights are not comparable to eight point lights with six faces each. |
| [Johannes Finn, *Evaluation of Performance and Image Quality for Voxel Cone Tracing*](https://www.diva-portal.org/smash/get/diva2:1637733/FULLTEXT01.pdf) — GPU timings profiled with Nsight Graphics on a laptop RTX 2060; Crytek Sponza, 1920×1080, 8× MSAA; one point light rendered to a 1024²-per-face cubemap using six 90° raster passes. | Shadow-map generation: **3.277–3.594 ms** across the tested voxel resolutions (256³–1024³). These are average frame timings; the table reports total frame times of 17.856–38.919 ms for its full VCT pipeline. | Most directly comparable light and map topology among these sources: one point light and six 1024² faces. It uses a different GPU, 1080p/8× MSAA, and legacy Crytek Sponza; the author does not report its triangle count or shadow culling policy. The rest of the VCT frame is unrelated to Krisp’s raster frame. |
| [Hannes, “Single Pass Omnidirectional Shadow Mapping Evaluation”](https://hannosprogrammingblog.blogspot.com/2015/10/) — author’s Sponza renderer on GTX 770; one point-light shadow map at 512² per face. It compares dual-paraboloid maps with a single-pass layered cubemap rendered through a geometry shader. | Dual paraboloid: **about 1.5 ms**. Layered cubemap: **about 8.3 ms**. The author explicitly says the dual-paraboloid path draws all scene geometry twice without view-frustum culling; the layered path emits each incoming vertex to cubemap faces through the geometry shader. | A useful direct example of shadow-map geometry amplification and path sensitivity, especially for the geometry-shader cubemap route. It is a single light at half Krisp’s face resolution, on a different GPU, and does not report triangle count or a total frame profile. The author attributes the large layered-path cost to the geometry shader, but this is one implementation and should not be generalized to other GPUs or geometry paths. |

## Interpretation

There is no matched published measurement here for Intel New Sponza, eight 1024² point-light cubemaps, RTX 2070, 1280×720, 1× shadow-pass MSAA, and Krisp’s per-triangle geometry-shader path. The references report 1.04 ms for 20 spot maps plus 1.30 ms for four sun cascades on GTX 970; 3.3–3.6 ms for one six-face point-light map on laptop RTX 2060; and 8.3 ms for one six-face, 512² layered geometry-shader cubemap on GTX 770. These are not a defensible expected time for Krisp: scene geometry, light type/count, resolution, GPU, and caster/culling work differ materially. The Hannes result also shows that implementation path alone can strongly affect the measured cost. The spot-map result is aggregate for a different light type; it cannot be divided by 20 and used as a per-point-light estimate.

Sources do not provide enough detail to normalize their timings for triangle count, draw count, visibility, culling, GPU-generation differences, or shadow-map pass implementation. Treat them as context for configurations and measured order of magnitude, not as evidence that Krisp is faster or slower.

## Why Krisp's measured cost is plausible

The measured generation cost is 6.15 ms for one light, 22.96 ms for four, and
47.24 ms for eight: approximately 5.7–6.2 ms per light. This agrees with repeated
work in the source rather than establishing an intrinsic per-light GPU cost.

- `src/graphics_engine/renderers/shadowmap_renderer.ipp:135–147` traverses the
  complete eligible caster list for each light. Its filters are the renderable
  visibility flag and the light's own object, with no spatial bounds test.
- `shaders/rasterization_shaders/shadow_map/geometry_shader.glsl:24–37` emits
  every input triangle to all six faces. Hardware clipping happens after this
  amplification, so a clipped copy still incurred earlier geometry work.
- The prepared asset has 115 mesh nodes referencing 115 unique meshes. Summing
  index accessor counts for its 401 opaque primitives gives 3,738,878 eligible
  caster triangles. Four blended primitives are excluded by
  `src/graphics_engine/render_draw_list.cpp:60–65`. These counts were verified
  directly from the pinned prepared glTF and its active scene traversal.
- Shadow resources are retained per in-flight frame and grow only when needed;
  their contents are cleared and redrawn on each frame. There is no map-content
  reuse even for this fixed scene and fixed light configuration.
- The 16-tap PCF loop is in `shaders/library/library.glsl:288–307`, called during
  main shading. It contributes to the 11.10 ms eight-light main span, while the
  dominant 47.24 ms generation span does not run that loop. Reducing main-pass
  MSAA or PCF taps therefore does not directly remove the measured generation
  workload.
- `shadow_map/fragment_shader.glsl:21–23` computes radial distance and writes
  `gl_FragDepth`. This may further limit early depth rejection; it is a profiling
  hypothesis, not a measured share of the 47.24 ms. Khronos explains the relevant
  [depth-output optimization restrictions](https://wikis.khronos.org/opengl/Fragment_Shader/Defined_Outputs).

The pass timestamps enclose GPU command-boundary spans, including device stalls;
they are not hardware-counter proof that geometry shading alone consumes all
that time. No factor-six error was found in the timing code. Geometry fanout,
fragment/depth work, and caching need controlled experiments to separate them.

## Unity and Unreal context

I did not find a matched published Unity or UE5 timing for this exact Intel New
Sponza/RTX 2070/eight-point-light workload. Official documentation explains why
engine features and settings matter:

- Unity HDRP renders six maps for a point light and offers dynamic resolution
  scaling. Its default update mode recomputes maps each frame; On Enable and
  On Demand modes reuse cached maps between updates. These are configurable
  capabilities, not evidence that a default Unity scene always caches shadows.
  Sources: [HDRP shadow configuration](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.3/manual/Shadows-in-HDRP.html),
  [shadow update modes](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.3/manual/shadow-update-mode.html).
- UE5 Virtual Shadow Maps render requested pages and cache them across frames
  until invalidated. Its documentation distinguishes geometry generation from
  shadow projection/filtering and describes frustum, page-mask and occlusion
  culling. Nanite geometry follows a different rendering path from conventional
  meshes. These mechanisms make a static-scene FPS comparison against Krisp's
  unconditional full redraw materially different work.
  Source: [Epic Virtual Shadow Maps documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine).

## Recommended next measurements

1. Compare 512² and 1024² faces with unchanged geometry, lights and filtering.
   A small generation-time change would strengthen a geometry-bound hypothesis;
   a large change would indicate significant pixel/depth/bandwidth cost.
2. Compare face-frustum-filtered rendering against six-face geometry-shader
   expansion. Preserve off-camera casters that can shadow visible surfaces;
   camera-frustum culling alone is not a correct shadow-caster filter.
3. Evaluate reuse for static lights and geometry, with explicit invalidation and
   separate cached-static and forced-update workloads. Retain the current
   uncached case as a stress measurement instead of hiding dynamic-update cost.

These are proposed experiments, not implemented optimizations or claimed gains.

## One-point-light controlled diagnosis

Controlled Release probes on the same RTX 2070 and normal graphics display used
Intel New Sponza, one fixed shadow-casting light, 1280x720 with 4x main-pass MSAA,
1 second of warm-up and 5 seconds of measurement, with two fresh workers per
variant. Validation was disabled. The unchanged baseline was repeated at the end
of each experiment. All 18 workers completed without lost records. These are
short attribution experiments rather than compatible standard-suite baselines.

| Variant | Shadow GPU time, ms | Interpretation |
| --- | --- | --- |
| Original 1024² faces | 6.048; final control 6.225 | Reproduces the original one-light cost. |
| `glslc -O` for shadow vertex, geometry and fragment shaders | 6.093 | No material improvement; source/SPIR-V complexity alone does not establish device cost. |
| Conservative cubemap-face triangle rejection, 1024² | 2.842 | Approximately 53% lower generation time than the initial control. |
| Projective-depth diagnostic, 1024² | 6.070 | Removing radial-depth computation/output does not explain the original dominant cost. |
| Face rejection plus projective-depth diagnostic | 2.644 | A smaller additional difference; radial-depth changes are not the primary lever. |
| Original shaders, 256² faces | 5.953; final control 6.058 | Sixteen times fewer texels leaves generation cost almost unchanged. |
| Face rejection, 256² faces | 2.619 | Geometry-side savings persist at much lower resolution. |

Numbers are the median of the two per-worker median GPU durations. Per-worker
medians for face rejection at 1024² were 2.841 and 2.844 ms, versus 6.093 and
6.003 ms for the initial baseline. Overall uncapped presents increased from
100.4–101.0/s to 150.2/s for that prototype. Whole-command GPU medians changed
from approximately 9.70 ms to 6.42 ms; main-pass medians remained about 3.3–3.4 ms.

The rejection prototype computes the same three clip-space vertex positions for
each cubemap face, then skips emission only if all three vertices are outside
one common homogeneous frustum plane. It still processes every input triangle
and computes six sets of transforms. The improvement therefore specifically
supports avoiding unnecessary geometry output and subsequent clipping as a
major lever. It does not quantify the remaining split between geometry-shader
invocation, transforms, output bandwidth and fixed-function clipping. Per-mesh
face culling before vertex processing or face-specific indexed rendering may
remove more work and require separate experiments.

The projective-depth variants deliberately leave the main shadow lookup using
radial-depth encoding, so their shadows are incorrect. They are diagnostic only
and are not proposed changes. The lower-resolution variants also change shadow
quality and are not proposed changes. The conservative rejection is intended to
preserve clipping results; production integration still needs full rendering
coverage beyond this static opaque workload.

Artifacts and per-variant shader hashes are in:

- `build/one-light-shadow-diagnostics/20261021T223335-9d27a2/results.json` (1024² probes).
- `build/one-light-shadow-diagnostics/20261021T223721-08a33b/results.json` (256² probes).

The throwaway harnesses are `build/diagnose-one-light-shadows.py` and
`build/diagnose-shadow-resolution.py`. Original shader binaries were restored
byte-for-byte, the resolution source was restored to 1024, and the original
Release configuration was rebuilt. No engine or shader source optimization was
left applied. This evidence supports a geometry-processing bottleneck for this
one-light workload, rather than shadow-map pixel count or radial-depth output
as the dominant explanation.

A follow-up geometry invocation probe used `layout(invocations = 6)` and
`max_vertices = 3`, with each invocation rendering one cubemap face. It produced
6.921 ms without rejection and 3.276 ms with rejection, compared with initial
and final original-path controls of 6.169 and 6.152 ms. Thus simply replacing
the per-invocation six-face loop with six independent geometry invocations was
slower on this GPU. It does not replace a test of face-specific indexed draws
without a geometry shader. The eight follow-up workers also completed without
lost records; the original binaries were restored byte-for-byte afterward.
Artifacts: `build/one-light-shadow-diagnostics/20261021T224222-23af70/results.json`.
Across all three probe groups, 26 Release workers completed. The supported first
optimization candidate remains conservative rejection before emitting triangle
copies; neither shader optimization flags nor lower shadow resolution addressed
the original cost, and geometry-shader invocation changes should not be assumed
to improve it without measurement.

Production integration subsequently applied the conservative test to all six
shadow geometry variants through a shared predicate. The integrated shader
measured 3.37 ms of shadow generation against a fresh 6.14 ms control; use that
result rather than the inline prototype's 2.84 ms for the shipped implementation.
Three static scene views were pixel-identical outside the FPS/TPS overlay, and
GPU execution of the predicate passed 5,016 correctness cases. Full conditions,
artifact paths, coverage limits and the existing instance-validation finding are
recorded in [PERFORMANCE.md](../../PERFORMANCE.md).
