# Close-up normal-map contrast investigation

## Finding

The saved wall view has two contributors to its harsh contrast: detailed
authored normals under grazing, direct-only lighting, and additional normal
distortion from the prepared BC3 textures. Shadow-map visibility is not needed
to reproduce it. Removing normal mapping removes the nearly black patches;
using the original PNG normal maps reduces them but does not remove them.

The investigation found no evidence of an additional normal-map sRGB,
coordinate-conversion, or tangent-handedness defect. This is a bounded finding,
not a claim that every normal-mapped mesh is correct. No production material
strength, light, texture format, or shader was changed by this investigation.

## Saved view and lighting

The camera position is `(5.9097996, 10.9922066, 1.1184869)`, looking toward
`(5.8898215, 10.7319202, 2.0838187)`. The scene has one white point light at
`(0, 12, 2)`, intensity 800, exposure EV 0, with shadow casting disabled.

A center-ray intersection with the prepared glTF identifies the opaque wall
as node 55, `arch_wall_2ndfloor`, primitive 2, material 16, `brickwall_01`.
Its normal texture is texture/image 39. A translucent `dirt_decal` lies in
front of it and has no normal texture. The wall intersection is approximately
`(5.87508, 10.53984, 2.79618)`, with surface normal `(0, 0, -1)` and
geometric N·L = 0.13039. The light is therefore only about 7.5 degrees above
the wall's tangent plane. Modest normal-map slopes can face away from it.

There is no active environment cubemap. In
`GraphicsEngine::configure_environment_lighting` the absent cubemap selects
neutral environment textures; their irradiance and specular contributions
are black. Component captures confirm zero environment and emissive light
in this view. The renderer has no constant ambient floor or indirect bounce
lighting to illuminate a normal facing away from the point light. Disabling
shadows removes shadow-map occlusion; it retains the BRDF's directional
response and the N·L hemisphere test.

## Controlled GPU comparison

Release captures on the RTX 2070 use the saved camera at 1280×720, with
shadows disabled. All variants retain the same non-normal textures, material
factors, light, exposure, and decal alpha behavior. The measured rectangle is
`(400, 200)`–`(960, 575)`. “Nearly black” means Pillow grayscale ≤ 2; this is a
comparison metric for this fixed view, not a general correctness threshold.

| Normal input | Mean grayscale | Grayscale standard deviation | Nearly black pixels |
| --- | ---: | ---: | ---: |
| Prepared BC3 normals, current mip chains | 98.76 | 68.38 | 11.36% |
| Original PNG normals, runtime mip chains | 120.31 | 64.52 | 6.58% |
| Geometric normals, either texture cache | 134.65 | 26.59 | 0.00% |
| Prepared BC3 normals, diagnostic scale 0.25 | 122.61 | 43.18 | 2.20% |

The geometric-normal captures agree exactly between the two caches, which
checks that the comparison did not change unrelated material inputs. The
quarter-strength result is diagnostic; it is not an adopted material fix.
Both normal representations still produce substantial contrast at full
authored strength.

The private PNG cache changes only the 23 normal texture bindings back to
their original source PNGs. It retains the prepared scene and all other
textures. Runtime PNG loading generates normal-vector mip chains. It does
not modify the pinned cache or the user's saved scene.

An early diagnostic incorrectly forced output alpha to one, making the dirt
decals opaque. Those component images are excluded from the comparisons.
Replacing only the final RGB sum with direct light, while retaining alpha,
matches the original image exactly. Zeroing emissive also matches exactly.
Subsequent diagnostic outputs preserve the original alpha.

## Texture quality and pipeline audit

An independent BC3 color-block decoder confirms that the compression loss
is in the prepared bytes, rather than a Pillow DDS display issue. Sampling
1,048,576 deterministic texels from each base level gives:

| Normal texture | RGB absolute error, 0–255 | Median angular error | 95th percentile angular error |
| --- | --- | ---: | ---: |
| `brickwall_01` (visible wall) | 7.72 / 7.55 / 3.13 | 2.19° | 22.86° |
| `arch_stone_wall_01` (separate material) | 34.99 / 28.14 / 10.71 | 19.13° | 60.40° |

Angular errors compare normalized decoded RGB vectors against normalized
source vectors. The independent decoder and Pillow differ by at most 1.37
channel units due to endpoint expansion/interpolation rounding. These
base-level statistics are not a measurement of the filtered normal at each
screen pixel. In particular, the worse arch-stone numbers must not be
attributed to the visible brick wall.

The audited engine path is internally consistent:

- glTF import reflects X and reverses triangle winding. Authored normal and
  tangent X are reflected, and tangent handedness is negated. This preserves
  the transformed bitangent under the reflection.
- Generated tangents use MikkTSpace on the converted geometry and original
  UVs. Model-space reflection is accounted for when transforming tangents.
- The fragment shader orthogonalizes the tangent against the interpolated
  normal, constructs the bitangent with the tangent sign, decodes RGB to
  signed components, applies normal scale to XY, and normalizes the result.
- Normal textures select `VK_FORMAT_BC3_UNORM_BLOCK`, rather than sRGB.
  The visible material has the default normal scale of 1.

Relevant implementations are `src/resource_loader/resource_loader_mesh.ipp`,
`src/resource_loader/resource_loader.cpp`,
`src/resource_loader/resource_loader_material.ipp`,
`src/graphics_engine/graphics_engine_texture_manager.ipp`, and
`shaders/rasterization_shaders/texture/{vertex_shader,fragment_shader}.glsl`.

## Recommended next work

Improve the normal-texture preparation quality with an angular-error quality
check and compare alternative encoders/formats against the original PNG
control. The current BC3 encoder causes substantial, material-dependent
distortion. A format change needs loader/upload/shader support and measured
storage and GPU-memory costs; BC5 cannot simply replace RGB normal data
without reconstructing Z.

For a less harsh scene, use the engine's environment-lighting support or
adjust scene lighting deliberately. Reducing normal strength can be an
artistic choice, but doing so globally would conceal compression errors and
change authored materials. This investigation does not justify another
blanket normal-sign flip or an artificial ambient term in the BRDF.

## Reproduction and evidence

The retained diagnostic harness is
`build/wall-lighting-investigation/probe.py`. Example invocation:

```sh
python3 build/wall-lighting-investigation/probe.py \
  --saved-camera /path/to/scene.yaml baseline geometric-normal
```

It temporarily substitutes the Release texture fragment SPIR-V, runs isolated
captures, and restores the original bytes in a `finally` block. Do not run it
concurrently with a shader build or another shader probe. The investigation
restored the original texture fragment SPIR-V SHA-256:
`c218bf6c786621ac43837edd23bdaf2425704bf38c19de4c38a71b890137f43c`.

Local evidence:

- BC3, geometric-normal, and quarter-strength results:
  `build/wall-lighting-investigation/20261022T165140-3043d7/`.
- Alpha-preserving direct-light equivalence:
  `build/wall-lighting-investigation/20261022T171142-0f8bdc/`.
- Original PNG and geometric-normal results:
  `build/wall-lighting-investigation/20261022T171503-4e5c38/`.
- Center-ray intersections: `build/normal-investigation/ray-hits.json`.
- Independent texture measurements: `build/normal-investigation/normal-quality.json`.
- Private PNG cache: `build/normal-investigation/png-normal-cache/`.

These are image-quality diagnostics, not benchmark throughput measurements.
Earlier mipmap aliasing and backlit-normal findings remain separate; see
`SPONZA_GRAIN_INVESTIGATION.md` and `BACKLIT_NORMAL_MAP_LIGHTING.md`.
