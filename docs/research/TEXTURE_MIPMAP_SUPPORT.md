# Texture Mipmap Support

Date: 2026-10-22

## Finding

The engine can upload and sample mip levels supplied in a texture asset. The
general material texture path does not generate spatial mip levels. PNG/JPEG
decoding produces one level, while BC3/DXT5 DDS assets may carry a complete or
partial authored chain. Several ordinary runtime image paths also create one
level, though lower-level callers can provide multi-level `TextureMaterial`
data. This is authored chain support, not generic mipmap generation.

## Asset and upload path

[`TextureMaterial`](../../src/renderable/material.hpp#L191) stores dimensions,
format, payload, and a byte size for each mip level in `mip_sizes`. The DDS
loader accepts DXT5/BC3 2D textures, checks dimensions and the maximum legal
level count, computes each level's BC3 block size, and rejects truncated data.
It retains the number of levels present in the file, so chains may be partial.
See [`load_dds_texture_data`](../../src/resource_loader/resource_loader_material.ipp#L46).
PNG/JPEG decode paths set a single mip size ([glTF image decode](../../src/resource_loader/resource_loader_material.ipp#L148),
[standalone image decode](../../src/resource_loader/resource_loader_material.ipp#L669)).

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
[`resolve_gltf_sampler`](../../src/resource_loader/resource_loader_material.ipp#L212).
The resource guide documents that authored DDS chains are sampled while
PNG/JPEG stay single-level ([`RESOURCES.md`](../RESOURCES.md#L160)).

## Coverage and limits

CPU/resource tests verify DDS mip-size decoding and imported sampler metadata,
including authored chains, each supported mip-filter interpretation, and
rejection of unsupported sampler modes
([DDS tests](../../test/resource_loader_tests.cpp#L1035),
[import tests](../../test/resource_loader_tests.cpp#L1086),
[sampler rejection tests](../../test/resource_loader_tests.cpp#L1300)).
The focused debug binary run passed 28 tests across four suites; it used the
existing binary without rebuilding. There is no GPU render validation in that
result, and the current tests do not prove visible LOD selection or filtering.

Benchmark asset preparation currently requires one-level DDS files and records
one runtime mip level ([validation/conversion](../../applications/stress_test/benchmark_assets.py#L603),
[metadata](../../applications/stress_test/benchmark_assets.py#L714)). This is a
constraint of that asset preparation path, not the engine DDS loader.

Proposed follow-up: if general material mip generation is needed, an offline
asset path is a simple extension. Filtering should account for texture meaning:
color data should be filtered in linear light, while normal maps need
vector-aware filtering and renormalization. An engine-side generator would
need equivalent semantic-aware behavior plus validation of format features
and supplied level dimensions.
