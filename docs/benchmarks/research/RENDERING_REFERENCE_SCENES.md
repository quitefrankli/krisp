# External rendering reference candidates

Research date: 2026-10-24. Research only: no assets downloaded, renderers run,
fixtures implemented, or approved baselines changed.

## Recommendation and scope

Start with Khronos's render-fidelity infrastructure, Avocado and small core glTF
tests, then add Flight Helmet using an explicitly shared material profile. These give
more interpretable evidence than a complex interior's beauty image. Reserve
Cornell Box, Blender Classroom and Bistro for later lighting capabilities.

External references help establish intended appearance and investigate mistakes.
They do not change Krisp's regression contract: every decoded RGBA8 channel and
dimension must exactly equal its approved Krisp baseline. Different renderers'
images are independent review evidence, not replacements for approved baselines
or a reason to introduce tolerant pass criteria. See
[RENDERING_FIDELITY.md](../RENDERING_FIDELITY.md).

Krisp currently exposes metallic-roughness PBR, base-colour, metallic-roughness,
normal and emissive maps; at most eight point lights; unshadowed environment
lighting derived from an sRGB skybox; and ACES-inspired presentation. Occlusion
bindings, transmission, clearcoat, area lights and indirect light transport are
outside this comparison profile. Ray tracing remains unsupported. These limits
make an unchanged HDR, area-lit, or transmissive reference an unsuitable exact
appearance target. See [GRAPHICS_ENGINE.md](../../GRAPHICS_ENGINE.md) and
[BENCHMARK_ASSETS.md](../BENCHMARK_ASSETS.md).

## Ranked shortlist

The order reflects usefulness for this engine now. Linked model READMEs contain
download links and source directories; select uncompressed glTF/GLB variants
rather than assuming every compressed variant is importable.

| Rank | Candidate and exact identifiers | Reference/setup and practical assessment |
| --- | --- | --- |
| 1 | **Khronos glTF Render Fidelity + Generator**, beginning with `khronos-Avocado` | Best existing comparison infrastructure: renderer adapters, scenario camera/environment settings, and public comparisons. Avocado is a small core-material entry before larger scenes. |
| 2 | **`MetalRoughSpheresNoTextures`, `MetalRoughSpheres`** | Factor and textured PBR charts isolate BRDF and channel/factor errors; useful alongside Krisp's `pbr-factors` fixture. |
| 3 | **`NormalTangentMirrorTest`, `NormalTangentTest`** | Authored correct/incorrect examples expose supplied tangent, generated tangent, handedness and normal-map convention mistakes. |
| 4 | **`AlphaBlendModeTest`, `NegativeScaleTest`** | Visible correctness markers test alpha modes, cutoff, winding, sidedness and transformed normals with less dependence on photographic lighting. |
| 5 | **`FlightHelmet`**, shared prepared profile | Rich material closeups; Krisp already pins and prepares it. Current upstream transmission and AO need a declared common subset. |
| 6 | **glTF Asset Generator**: `Material_MetallicRoughness`, `Material_AlphaMask`, `Material_DoubleSided`, `Node_NegativeScale` | Narrow importer/feature matrix; select individual cases rather than importing its whole suite. |
| 7 | **Cornell Box measured dataset** | Excellent future physical-lighting reference; area illumination, interreflection and spectral measurements currently exceed Krisp's model. |
| 8 | **ORCA Bistro**, optionally PBRT's `bistro/` conversion | Detailed scene with downloadable geometry and camera/light descriptions; conversion, lighting and scale make it a later candidate. |
| 9 | **Blender Classroom** | An official downloadable Cycles demo with its native scene setup. Useful for future interior lighting comparisons; glTF conversion and matching light transport need a separate audit. |

### 1. Current cross-renderer infrastructure

Google's [old fidelity page](https://modelviewer.dev/fidelity/) now points to
Khronos ownership. Use the [current comparison site](https://github.khronos.org/glTF-Render-Fidelity/)
and [Generator](https://github.com/KhronosGroup/glTF-Render-Fidelity-Generator),
not assumptions about the old Google package. The current
[configuration](https://github.com/KhronosGroup/glTF-Render-Fidelity-Generator/blob/main/test/config.json)
names model-viewer, Filament, Babylon, glTF Sample Viewer, three-gpu-pathtracer,
STELLAR, V-Ray and Blender Cycles. It specifies model paths, orbit/target,
dimensions, environment overrides and renderer exclusions. Availability of an
adapter does not establish that every renderer supports every scenario or is
installed locally.

`khronos-Avocado` has explicit framing. Its
[asset README](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Avocado/README.md)
provides GLB and a CC0 notice crediting Microsoft. The inspected
[uncompressed glTF](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Avocado/glTF/Avocado.gltf)
uses base-colour, metallic-roughness and normal maps, UV set 0 and supplied
tangents, with no AO binding or extensions: a particularly close material fit.
The generator's
[Cycles script](https://github.com/KhronosGroup/glTF-Render-Fidelity-Generator/blob/main/test/renderers/blender-cycles/render.py)
implements camera conversion, HDR environment loading, rest-pose selection and
explicit postprocessing, including exposure scaling and ACES/gamma conversion.
It uses doubled output dimensions and denoising: copying nominal camera values
alone will not recreate its pixels. Match the resolved setup and pin versions.

Generator code is [Apache-2.0](https://github.com/KhronosGroup/glTF-Render-Fidelity-Generator/blob/main/LICENSE).
That does not establish blanket reuse rights for each HDR environment or public
render. Individual rendered-image grants were not verified; prefer generating
local reference images from assets/environments with retained notices. Public
catalogue inclusion also does not prove an asset has completed images from
every adapter.

[DamagedHelmet](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/DamagedHelmet/README.md)
is less straightforward despite its popularity: current notices include
CC BY 4.0 for conversion and CC BY-NC 4.0 for the earlier model. Do not treat it
as an unrestricted CC BY alternative to Avocado or Flight Helmet.

### 2–4. Small semantic reference assets

[MetalRoughSpheresNoTextures](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/MetalRoughSpheresNoTextures/README.md)
is CC0; [MetalRoughSpheres](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/MetalRoughSpheres/README.md)
is CC BY 4.0, credited to Analytical Graphics/Ed Mackey. Both provide GLB.
Use the factor-only chart first, then the texture chart with identical
environment, framing and presentation. This recommendation follows the assets'
declared testing roles, not a verified engine import.

[NormalTangentMirrorTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/NormalTangentMirrorTest/README.md)
supplies tangents and is CC BY 4.0.
[NormalTangentTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/NormalTangentTest/README.md)
omits tangents and requires their generation; its current README states CC0.
Both recommend a clear horizon environment and document front/back expectations.
Start with the supplied-tangent test. Audit material bindings, including AO,
and importer requirements before calling either fixture ready.

[AlphaBlendModeTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/AlphaBlendModeTest/README.md)
documents OPAQUE/BLEND/MASK, cutoff values and visible failure examples.
[NegativeScaleTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/NegativeScaleTest/README.md)
documents determinant-dependent winding, culling and normal behaviour. Both
provide GLB and CC BY 4.0 notices. These are particularly useful because expected
checkmarks and normal orientation remain interpretable across shading choices.

The Khronos catalogue lists all these assets, but the inspected generator
`main/test/config.json` did not contain their names. Verify the published
comparison's actual resolved configuration or create a shared one; do not claim
a catalogue screenshot is a reproducible matched render. Asset notices are
verified; separate screenshot reuse grants were not established.

### 5–6. Rich materials and generated coverage

[Current Flight Helmet](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/FlightHelmet/README.md)
is CC0 and declares transmission; uncompressed glTF and separate compressed
variants exist. Krisp's pinned recipe removes AO, makes lenses opaque and uses
lossy BC3 textures. Feed that same derived material/texture profile to the
reference renderer, recording supported decode/import handling, or compare an
explicitly identical source profile separately. An upstream screenshot cannot
validate those material changes. Krisp's existing pin is not the current
upstream revision.

[Asset Generator](https://github.com/KhronosGroup/glTF-Asset-Generator)
provides downloadable generated assets and property tables. For
[Material_MetallicRoughness](https://github.khronos.org/glTF-Asset-Generator/Output/Positive/Material_MetallicRoughness/),
start with cases `00`, `02`, `03`, `04`, `05`, `06`, `08`, `09`, `10`: these
isolate defaults, textures and factors without selecting its vertex-colour
cases. Repository code/documentation is
[MIT](https://github.com/KhronosGroup/glTF-Asset-Generator/blob/main/LICENSE);
retain packaged notices and inspect generated asset/texture notices before
redistribution. Its sample thumbnails are importer illustrations, not a
verified camera/environment/exposure reference set.

### 7–9. Later lighting targets

[Cornell's current data page](https://bowers.cornell.edu/computer-graphics/data)
contains measured geometry, camera parameters, reflectance and light spectra,
photographs in TIFF/OpenEXR and synthetic JPEG/RGBE links; several downloads
route through archived URLs. The [experiment description](https://bowers.cornell.edu/cornell-box)
explains comparison against a calibrated physical capture. Data/image reuse
terms and archive download integrity remain unverified. A point-lit adaptation
would be a different experiment and cannot reproduce the original area-light
and colour-bleeding reference.

[ORCA Bistro](https://developer.nvidia.com/orca/amazon-lumberyard-bistro)
offers FBX/Falcor under CC BY 4.0, with approximately 1.05 million interior and
2.83 million exterior triangles. [ORCA](https://developer.nvidia.com/orca)
says included Falcor scenes contain camera and lighting configuration, and
labels the archive legacy/unsupported. [PBRT's scene repository](https://github.com/mmp/pbrt-v4-scenes)
also supplies a `bistro/` conversion with reference imagery and CC BY credit.
Neither is a verified glTF import path or a matched Krisp exposure/lighting
setup. Separate image grants were not verified.

[Blender's official demo page](https://www.blender.org/download/demo-files/)
lists Christophe Seux's Classroom under Cycles, with a 72 MB download and CC0
notice. Its [official download page](https://www.blender.org/download/demo/test/classroom.zip/)
links the scene archive. This supplies a native Blender scene rather than just
a screenshot, making it a useful future source of camera, lighting and material
setup. No archive inspection, glTF export, material audit or matched Krisp render
was performed here; the CC0 scene notice is not a verified grant for every
independently published screenshot. Given Krisp's current point-light and IBL
limits, treat matching its interior illumination as a future capability target.

Do not confuse Krisp's Intel Sponza cache with
[Khronos Sponza](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md):
the latter identifies a Cryengine Limited License and explicitly says its
screenshot lights are absent from the model. Reusing the scene name does not
establish matching geometry, materials, camera or illumination.

## Evidence needed before implementation

Pin asset, generator, renderer and environment revisions; record notices and
hashes separately for assets, environments and reference images. Resolve camera
projection, transforms, world scale, light units, environment orientation and
dynamic range, texture colour spaces, sampler/mip settings, exposure, tone
mapping, encoding, resolution and antialiasing. Krisp's LDR-derived IBL is a
material blocker for reproducing an HDR-lit public image; a common LDR
environment can support a newly generated comparison, without claiming the
original lighting was reproduced. Review independent renders first; preserve
Krisp's existing explicit baseline approval and exact equality checks.

## Implementation validation (2026-10-24)

The imported suite now prepares eight unmodified, checksum-pinned Khronos
assets and nine captures. Six imported captures render repeatably in two fresh
Krisp processes: Avocado, both metallic/roughness charts, NegativeScaleTest,
and the three-person CesiumMan scene in bind and independently sampled poses.
The other three cases deliberately report importer errors for unsupported
`occlusionTexture`: AlphaBlendModeTest, NormalTangentTest, and
NormalTangentMirrorTest. Their source materials have not been stripped.

Independent references were generated for all nine captures in the official
Khronos renderer (`@khronosgroup/gltf-viewer` 1.1.0 on the RTX 2070 OpenGL driver)
and the locally installed Blender 5.2.2 LTS Cycles. Reference images are human
review evidence; differences in light-unit conversion, IBL filtering, display
transforms, material sidedness, path tracing, and shadow support prevent treating
one renderer's output as another renderer's exact expected pixels. Khronos has
no cast shadows; Cycles reference images use 32 CPU samples without denoising.
The reference provenance records these settings and all source/image hashes.

Visual review found a specific Krisp discrepancy: the negative-scale chart
shows failed reflected front/back checks where Khronos shows green checks.
This is evidence for investigating reflected-transform culling, not a reason
to approve the current Krisp image. Cycles also differs on sidedness checks,
so the Khronos raster result is the useful comparison for that diagnostic.
The people captures show three independent poses and Krisp shadows; all three
renderers preserve the requested compositions and pose times.

The 12 original captures still match their approved PNG pixels exactly, checked
separately after the sampler implementation. Their normal verdict is currently
`incompatible` because the worker fixture/settings identity changed. New
captures remain `approval-required`; no approved baseline was changed. The full
GPU invocation returns failure because unsupported material cases and missing
or incompatible approvals are failures. All 10 regular Meson suites passed.
