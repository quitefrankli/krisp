# Texture Mipmap Support

Date: 2026-10-22

## Finding

The engine uploads and samples supplied mip chains. Following this investigation,
PNG/JPEG textures loaded with the `NORMAL` semantic also receive a complete mip
chain during decoding. Normal vectors are filtered and renormalized, with positive
Z as the fallback for cancellation; the original base pixels remain unchanged.
Other PNG/JPEG semantics remain single-level. BC3/DXT5 DDS assets preserve their
supplied complete or partial chains without runtime generation.

The benchmark converter now produces normal-map mip chains before BC3
compression, using the `engine-pbr-opaque-bc3-normal-mips-v1` profile. It uses
Pillow BOX filtering; the CPU loader uses exact fractional source-area weights
for odd dimensions. Both normalize filtered vectors and average alpha linearly.
These reducers need not produce identical bytes for non-power-of-two images.

## Asset and upload path

[`TextureMaterial`](../../src/renderable/material.hpp#L191) stores dimensions,
format, payload, and a byte size for each mip level in `mip_sizes`. The DDS
loader accepts DXT5/BC3 2D textures, checks dimensions and the maximum legal
level count, computes each level's BC3 block size, and rejects truncated data.
It retains the number of levels present in the file, so chains may be partial.
See [`load_dds_texture_data`](../../src/resource_loader/resource_loader_material.ipp).
Both glTF and standalone decoded-image paths share the normal-mip generator
and RGBA8 storage helper in that file.

The texture manager creates the sampled image view across every supplied level
([`create_texture`](../../src/graphics_engine/graphics_engine_texture_manager.ipp#L269)).
Image allocation uses the supplied level count and transfer-destination plus
sampled usage. It transitions all levels, stages their bytes, and transitions
them for shader access in [`create_texture_image`](../../src/graphics_engine/graphics_engine_texture_manager.ipp#L418).
[`stage_data_to_image`](../../src/graphics_engine/resource_manager/graphics_buffer_manager.ipp#L283)
builds one `VkBufferImageCopy` region per supplied level, using mip-major byte
offsets and dimensions clamped to at least 1. This path does not allocate a
transfer-source image or issue mip-generation blits. Image creation, views, and
layout barriers carry the level count through the Vulkan helpers
([`create_image`](../../src/graphics_engine/graphics_engine.cpp#L629),
[`create_image_view`](../../src/graphics_engine/graphics_engine.cpp#L695),
[`transition_image_layout`](../../src/graphics_engine/graphics_engine.cpp#L723));
the barrier still has an old comment describing the image as having no mip
mapping at line 740.

The loader validates DDS level byte sizes, but the lower-level GPU upload path
accepts a `TextureMaterial` directly and uses its `mip_sizes` to size copy
regions and offsets without repeating equivalent dimension/count/payload
validation ([staging upload](../../src/graphics_engine/resource_manager/graphics_buffer_manager.ipp#L283)).
This is a validation boundary gap for programmatically constructed materials;
no resulting failure was reproduced.

The ordinary cubemap path uses default one-level images
([texture manager](../../src/graphics_engine/graphics_engine_texture_manager.ipp#L476));
the texture compositor also creates a default one-level image
([texture compositor](../../src/graphics_engine/texture_compositor.cpp#L125)).
Environment lighting is a separate exception: CPU processing computes a full
roughness-prefiltered specular cubemap chain with GGX filtering
([`generate_prefiltered_specular`](../../src/graphics_engine/environment_map_processor.cpp#L340)),
then uploads the processed levels through its dedicated environment texture
path ([manager upload](../../src/graphics_engine/graphics_engine_texture_manager.ipp#L333)).
That chain encodes roughness filtering for image-based lighting; it is not
generic spatial mip generation for material images. The shader selects its
level explicitly from roughness ([`library.glsl`](../../shaders/library/library.glsl#L230)).

## Samplers and shader lookup

[`PbrMaterial::TextureSampler`](../../src/renderable/material.hpp#L34) records
repeat or clamp-to-edge addressing and NONE, NEAREST, or LINEAR mip selection.
The Vulkan sampler uses linear magnification/minification texel filtering,
maps the requested mip interpolation mode, and sets max LOD to zero for NONE
or unrestricted for mip filtering ([sampler creation](../../src/graphics_engine/graphics_engine_texture_manager.ipp#L552)).
Ordinary material shaders call GLSL `texture()` and rely on implicit derivatives
for LOD selection ([base color](../../shaders/library/library.glsl#L23),
[metallic-roughness and normal](../../shaders/rasterization_shaders/texture/fragment_shader.glsl#L51)).
The supplied image view limits sampling to the levels the asset actually has.

The glTF importer defaults a missing sampler to repeat with linear mip
filtering. It maps `LINEAR_MIPMAP_LINEAR` (also the omitted minification
default) to linear mip interpolation, `LINEAR_MIPMAP_NEAREST` to nearest mip,
and `LINEAR` to no mip filtering. It accepts only linear texel filtering,
matching S/T axes, and repeat or clamp-to-edge addressing; unsupported settings
on used samplers fail import. See
[`resolve_gltf_sampler`](../../src/resource_loader/resource_loader_material.ipp#L327).
The resource guide documents authored DDS chains and generated PNG/JPEG normal
chains ([`RESOURCES.md`](../RESOURCES.md#L160)). Explicit no-mip samplers still
limit sampling to level zero.

## Coverage and limits

CPU/resource tests verify DDS mip-size decoding and imported sampler metadata,
including authored chains, each supported mip-filter interpretation, and
rejection of unsupported sampler modes
([resource-loader tests](../../test/resource_loader_tests.cpp)).
Loader regression tests also cover normal-map mip generation through standalone
and glTF image paths, odd-size edge contribution, and cancellation fallback.
These CPU tests do not prove visible GPU LOD selection or filtering.

Benchmark preparation requires complete normal chains and single-level images
for other slots. Images shared by normal and other slots receive separate
variants. Validation checks DDS chain sizes and records mip counts and total
BC3 payload bytes ([validation/conversion and metadata](../../applications/benchmarks/performance/benchmark_assets.py)).
Recognized old caches migrate from verified sources on the next preparation;
old prepared textures are not modified merely by rebuilding the engine.

General color and material-data mip generation remains a separate follow-up.
Normal-map generation adds load/preparation work and texture memory; see
[`PERFORMANCE.md`](../PERFORMANCE.md) for the cost model and measurement limits.

## Fix validation

The Debug engine suite passed 433 tests with one display-dependent test skipped.
The asset-preparation suite passed 24 tests, and the benchmark-runner suite
passed 32. Both Debug and Release targets build successfully. Regenerating the
production cache validated all 91 pinned files: 28 normal DDS paths and the two
glTF files changed; all other runtime-file hashes stayed unchanged.

The existing Release render probe, `python3 build/investigate-grain.py baseline
--check-clean`, passes on the regenerated cache. On the RTX 2070, high-contrast
pixels in the fixed arch patch decreased from 26.038% in the restored original
cache to 7.954%. The wall patch decreased from 3.880% to 1.286%. This checks the
identified minification artifact in one view; it does not establish final
rendering quality or a performance improvement. Evidence:

- Original cache: `build/grain-investigation/20261022T131332-2b0802/results.json`
- New recipe: `build/grain-investigation/20261022T135806-3fab12/results.json`
- New capture: `build/grain-investigation/20261022T135806-3fab12/baseline/capture.png`
