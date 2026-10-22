# Sponza wall grain investigation

## Finding

The strong black-and-white wall grain is mainly caused by sampling detailed
normal maps without mipmaps. The effect remains with shadows disabled. On the
controlled RTX 2070 capture, adding a full mip chain to the normal maps reduces
the high-contrast pixel proxy from 26.04% to 7.95%. Disabling normal maps brings
it to 6.76%. The remaining 7.95% is close to the no-normal result, so missing
normal-map mip levels explain most of the visible grain in this view.

This is a correctness investigation, not a performance benchmark. Captures used
an unshadowed Sponza atrium at 1280×720 with 4× MSAA. The controlled shader
variants used the same fragment shader apart from the requested shading
variation. The right-arch ROI was (880, 250)–(1080, 500); its diagnostic proxy
counts pixels where luma differs by more than 50 from a 3×3 box blur. This also
counts real edges, so the score is useful for comparing these captures, not a
general image-quality measure. A separate wall-patch proxy fell from 3.88% to
1.27% with full normal mips and to 1.25% with normal maps disabled.

## Controlled results

| Variant | Right-arch high-contrast proxy | Interpretation |
| --- | ---: | --- |
| Baseline, single-level BC3 normal maps | 26.04% | Strong grain |
| Normal maps disabled | 6.76% | Grain mostly disappears |
| Roughness forced high and metalness disabled | 25.69% | Specular highlights are not the primary cause |
| Original PNG normal maps, base level only | 26.70% | BC3 compression is not the primary cause |
| BC3 normal maps with 13 levels (4096→1) | 7.95% | Mip filtering removes most of the grain |
| Original normal PNGs prefiltered to 512×512 | 10.68% | Filtering also helps without BC3 compression |

The no-normal variant retains base color and the roughness/metalness path. The
roughness/metalness variant retains the normal maps. The PNG comparison changes
the normal source format while keeping base-level-only sampling; it does not
show a material improvement. Thus BC3 normal compression is a separate quality
loss, but does not explain the visible grain as well as the missing mip levels.

## Texture and engine path audit

The pinned cache contains 72 referenced Sponza images. Their source PNG and
prepared DDS hashes all match `build/benchmark-assets/prepared.json`. The
source and prepared glTF keep the same material texture indices. In
`applications/stress_test/benchmark_assets.py:635-691`, preparation converts
each PNG to RGBA and writes a single-level BC3/DXT5 DDS with Pillow 11.3.0. It
does not swizzle or repack normal-map channels. Normal maps inspected have the
expected tangent-space pattern: red and green average near 128, blue near 240.

Base-color decoding is close to the source (arch stone sample RGB MAE 0.80,
99th percentile absolute error 3). BC3 loses more detail in several noisy
normal maps: full-resolution arch-stone R/G/B MAE is 36.3/28.2/10.5, while
brickwall_01 is 8.0/7.5/3.0. These losses are real and vary by image, but the
base-level PNG control still scores 26.70%, essentially the same as baseline.
The audit preview and channel statistics are in
`build/grain-texture-audit/stone_wall_normal_qa.png` and
`build/grain-texture-audit/metrics.json`.

The preparation validator explicitly rejects DDS files with more than one
mip level (`_dds_info`, around line 622), and the PNG conversion writes only a
base level (`_convert_png_to_dds`, around line 635). In contrast, the engine
already loads and uploads multi-level DDS images, and the scene sampler uses
minification filter `9987` (`LINEAR_MIPMAP_LINEAR`). The mismatch is in asset
preparation: the sampler requests mip filtering, while the prepared textures
provide no lower-resolution levels.

## Recommendation

Update asset preparation to produce semantic normal-map mip chains, including
normal-vector filtering and renormalization, then record the mip count and
recipe in the converter profile and prepared-file hashes. Keep the existing
runtime DDS path. Do not disable normal mapping or add runtime PNG decoding.
The full-mip fixture adds 128,625,936 BC3 bytes over the current 385,875,968
base-level bytes (about 33%); account for this real memory increase when
adopting the recipe. This investigation makes no performance-improvement
claim. Choosing a higher-quality normal-map compression format such as BC5
would be a separate change and is outside this diagnosis.

The mipmapped capture still has some coarse dark patches, and the fixed-size
PNG control sacrifices close-up detail. Neither fixture establishes final
rendering quality. Validate the production mip recipe across views, retain
normal detail at close range, and assess the remaining compression/lighting
artifacts separately. Base-color and other material textures also currently
lack mip chains; their semantic filtering needs coverage in the final recipe.

## Evidence and limits

GPU captures and result records:

- Baseline: `build/grain-investigation/20261022T130136-2cf0b9/`
- Shading A/B and diagnostic views: `build/grain-investigation/20261022T130235-fc57a1/`
- Full normal-map mip chain: `build/grain-investigation/20261022T130724-55edf8/`
- Original PNG normal maps: `build/grain-investigation/20261022T130733-f066c2/`
- Restored original-cache confirmation: `build/grain-investigation/20261022T131332-2b0802/`
- Detached mip-fixture confirmation: `build/grain-investigation/20261022T131340-adbeb8/`
- Prefiltered PNG control: `build/grain-investigation/20261022T131349-91dc74/`

The automated reproduction command is
`python3 build/investigate-grain.py baseline --check-clean`. It exits 1 with
26.04% high-contrast pixels on the restored original cache. Setting
`KRISP_GRAIN_ASSET_CACHE` to the absolute path of
`build/grain-sampling-audit/full-normal-mips` makes the same command exit 0 with
7.95%. The diagnostic threshold is 12% in the fixed right-arch ROI; this checks
the reported heavy speckling, not general visual correctness. The prefiltered
PNG control also passes. All diagnostic renders use Release on the RTX 2070;
the isolated X display is not suitable for throughput comparisons.

One fixture-generation attempt temporarily rewrote 23 symlinked prepared DDS
files. Those files were restored byte-for-byte, and all 91 prepared files were
rechecked against the manifest; see
`build/grain-sampling-audit/restoration.json`. The contaminated control run
`20261022T131016-8a0201` was excluded. No source fix or lasting prepared-cache
change is part of this investigation. A separate 512-filtered PNG normal-map
fixture confirms the filtering result independently of BC3. All 76 pinned
Sponza source-file hashes were also verified after the investigation. The
production fragment shader was restored to its exact original hash.
