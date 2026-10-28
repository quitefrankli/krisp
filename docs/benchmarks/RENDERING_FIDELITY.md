# Rendering fidelity

The fidelity mode of the `benchmarks` application checks the final pixels
produced by the production Vulkan renderer on the current GPU and driver. The
performance mode measures performance. The fidelity suite compares every
decoded RGBA8 channel and the image dimensions exactly. A single changed channel
fails comparison; there are no tolerances, masks, or perceptual pass criteria.
PNG compression and ancillary metadata do not affect the verdict.

## Running

From the repository root, with the normal Conan dependencies, a working Vulkan
device, Xvfb (`xvfb-run` and `xauth`), and Python 3 with Pillow installed:

```sh
./run benchmarks -- fidelity run --report
./run benchmarks -- fidelity run --case pbr-factors --report
```

The launcher builds the Debug application by default. Performance mode always
uses an optimized Release build with assertions disabled. `--release` selects
a separate fidelity build/profile; its baselines do not apply to Debug. No
software renderer is installed or pinned by this suite. Each run starts one
private Xvfb display shared by its workers, so their windows do not appear on your desktop
or steal focus. Vulkan still uses the hardware GPU; Xvfb supplies the display.
The virtual server is cleaned up when the run ends. Help, proposal preparation,
and promotion do not start a display. Missing virtual-display dependencies fail
with an actionable diagnostic instead of falling back to visible windows.
A restricted sandbox may still need permission to access the GPU outside it.

Each selected capture runs in two fresh processes. The worker freezes a defined
scene state, captures an identified immutable snapshot after presentation
processing, and excludes UI, diagnostic quads, and performance overlays. It
waits for resources and capture completion rather than sleeping for an assumed
warm-up interval. All required variants must complete. The two captures must
match each other before baseline comparison can pass.

The large textured scene uses the existing prepared asset cache. Prepare it
explicitly before a full fidelity run:

```sh
./run benchmarks -- performance prepare-assets
```

Preparation verifies pinned sources and preserves licences. Fidelity verifies
the prepared cache before launching its textured-environment workers. Workers
do not download assets, and missing or corrupt assets fail the affected case
without substituting geometry or silently skipping it.

The imported glTF cases use a separate pinned cache. Prepare it before a full
suite run:

```sh
./run benchmarks -- fidelity prepare-assets
```

## Independent renderer references

The suite includes eight imported glTF cases with nine captures. These cases
exercise official Khronos assets through the production renderer, including
material, normal/tangent, transparency, and negative-scale behavior. Avocado
and the textureless metallic/roughness asset use explicit uniform scene
normalizations to keep their very small source geometry beyond the runtime
camera near plane; their source geometry is unchanged. The people case contains
three separate CesiumMan instances. Its `bind` capture uses the bind pose;
`posed` samples source clip 0 at 0.25, 0.5, and 0.75 seconds. Both use the
declared camera, LDR horizon environment, floor, and shadow-casting point light.

Independent images are optional review evidence from the official Khronos glTF
Sample Renderer and Blender Cycles. Install the Khronos web package and run the
reference command:

```sh
npm install --prefix applications/rendering_fidelity/reference_web --no-audit --no-fund
./run benchmarks -- fidelity render-references --output /absolute/path/to/references
./run benchmarks -- fidelity run --references /absolute/path/to/references --report
```

Reference generation defaults to both renderers. It requires Blender for
Cycles, and Node/npm plus the Python Playwright browser dependencies for
Khronos. Khronos does not cast shadows, so its people capture is marked with
that limitation; Cycles renders on CPU. Both adapters consume each case's
declared camera, instances, poses, point lights, floor, and LDR environment.
Chrome runs headlessly using the current GPU's OpenGL driver and disables its
software rasterizer fallback; it does not create desktop windows. Reference
jobs have a five-minute deadline. Offline verification of existing references
does not launch either renderer or require their optional dependencies.
Differences in renderer features and display pipelines make these images
useful for inspection rather than pixel-level expected output.

The reference manifest records renderer versions, source asset commit and
file hashes, case and scene hashes, effective camera/render settings,
environment conversion details, image hashes, and renderer limitations. The
HTML report displays Current Krisp and independent renders side by side in a
separate review-only section, with provenance and limitations. Reference errors
are reported separately and never change the exact capture verdict; invalid
reference evidence can still make the command return nonzero.

Runs with references do not write or refresh approved baselines. New captures
without approved baseline images remain `approval-required`. Review the
captures, report, provenance, and limitations before preparing a proposal.
Promotion continues to require explicit user approval of the concrete
proposal; independent renders remain review evidence and are not promoted as
expected output.

## Cases

The default suite contains 16 cases and 21 captures. The initial viewport is
640 by 480. Each capture records effective camera,
exposure, material, environment, light, shadow, and scene settings along with
the fixture and prepared-asset identity.

| Case | Capture IDs | Coverage |
| --- | --- | --- |
| `colour-presentation` | `exposure_m1`, `exposure_0`, `exposure_p1` | Unlit colour/texture patches, emissive region, bright lit HDR region, exposures -1/0/+1 EV |
| `pbr-factors` | `grid` | Metallic and roughness combinations under fixed direct lighting |
| `textured-normals` | `all_maps` | Base-colour, metallic/roughness, emissive and tangent-space normal textures |
| `environment-lighting` | `ibl_roughness` | Fixed environment with diffuse/specular IBL and roughness variation |
| `point-shadows` | `shadowed`, `unshadowed` | Multiple coloured lights and fixed caster/receiver geometry, with shadow variants |
| `visibility-alpha` | `depth_sidedness_alpha` | Depth overlap, sidedness, checker alpha mask and overlapping blends |
| `hierarchy-skinning` | `bind_pose`, `animated_pose` | Nested transforms and a two-bone strip, with a fixed 0.65 radian tip rotation in the animated pose |
| `textured-environment` | `sponza_atrium` | Fixed Sponza atrium view with textured materials and multiple shadow-casting lights |
| `reference-avocado` | `default` | Imported official Avocado asset with explicit 100× scene normalization |
| `reference-metal-rough` | `default` | Imported official textured metallic/roughness sphere scene |
| `reference-metal-rough-textureless` | `default` | Imported metallic/roughness factors without textures, with explicit 1000× scene normalization |
| `reference-normal-tangent` | `default` | Front-facing normal-mapping tangent-generation test and horizon reflections |
| `reference-normal-tangent-mirror` | `default` | Front-facing mirrored tangent-space test and horizon reflections |
| `reference-alpha-blend` | `default` | Imported alpha blend and mask material test |
| `reference-negative-scale` | `default` | Imported negative-scale and winding test |
| `reference-people` | `bind`, `posed` | Three independent CesiumMan actors with fixed bind and animated poses, floor, and shadows |

Small fixtures and their procedural worker source belong to this repository
and are hashed as fixture inputs. The Sponza case uses the declared
prepared opaque PBR material profile and verified files from
`build/benchmark-assets`; it does not claim support for source material features
removed by that profile. See [BENCHMARK_ASSETS.md](BENCHMARK_ASSETS.md) for the
recipe and attribution.

## Evidence and outcomes

The alpha and normal/tangent reference assets currently expose unsupported
`occlusionTexture` material inputs. Their Krisp workers report an explicit
`error`; they are not simplified or counted as passing. Independent renderer
images can still be generated for these cases. This keeps the missing material
capability visible while Avocado, the sphere charts, negative-scale checks, and
the people scene exercise the supported import path.

Runs create unique evidence directories under
`$XDG_DATA_HOME/krisp-engine/rendering-fidelity` (falling back to
`~/.local/share/krisp-engine/rendering-fidelity`). The command prints the actual
path. Evidence includes captures from both processes, worker logs/manifests,
source provenance, effective environment/settings, and `run_manifest.json`.
Independent cases continue after failures; available evidence is preserved.

| Status | Meaning |
| --- | --- |
| `pass` | Complete, repeatable, compatible captures exactly match approved images |
| `mismatch` | At least one decoded channel or dimension differs |
| `nondeterministic` | Fresh-process captures or their effective settings disagree |
| `approval-required` | Valid captures have no approved baseline |
| `incompatible` | Approved evidence cannot be compared under the current profile/content |
| `error` | Capture, prerequisite, timeout, evidence, or execution failure |

Only a run in which every requested capture passes returns success. Missing
baselines never count as passing. GPU/driver, build class, validation state,
effective settings, formats, and fixture/asset identity participate in
compatibility. Source revisions and shader changes can differ: those are the
changes the suite is intended to detect. Provenance records the revision and
dirty source identity rather than using them to force a fresh baseline.

`--report` creates a local HTML comparison of reference, current, and difference
images, with changed-pixel counts and bounds. Amplified differences are labelled
diagnostics. Reports include missing-image explanations and repeatability
evidence. They require no external network resources and do not change image
verdicts or baseline files. A report-writing error is separate from a recorded
image comparison result.

## Reviewing and approving baselines

Normal runs never write approved baselines under
`test/rendering_fidelity/baselines`. To prepare reviewable evidence:

```sh
./run benchmarks -- fidelity propose /absolute/path/to/run-directory
```

This writes a baseline proposal identifying exact cases, captures, image
digests, environment/profile, and expected existing baseline set. Preparing a
proposal does not approve it. Review the report and manifest before promotion:

```sh
./run benchmarks -- fidelity promote /absolute/path/to/baseline-proposal.json
```

Promotion requires an interactive confirmation of the exact proposal digest.
There is no automatic accept/update-on-failure switch. Initial baselines and
intentional appearance changes both require explicit user approval. Agents must
present the concrete evidence and obtain that approval before executing
promotion; a request to implement or test the suite is not baseline approval.
A terminal or approval-looking file cannot independently prove who approved it;
the user approval requirement is also a workflow boundary.

Candidates must be complete and repeatable, but need not match the old baseline.
Promotion rechecks evidence and the expected destination state. If candidates,
settings, or existing baselines changed after review, generate a new proposal
and obtain fresh approval. A failed promotion must not activate a partial set.
Keep old sets for recovery and review baseline changes in version control.

## Test integration

CPU tests for capture coordination, image comparison, runner failures,
promotion integrity, and reporting are part of the normal Meson tests. GPU
fidelity is explicit:

```sh
meson compile -C build/debug -j 6 benchmarks_fidelity_gpu
```

That target runs the suite and returns nonzero when graphics, assets, or
approved baselines are unavailable. Ordinary unit tests do not require those
GPU prerequisites. Exact matching checks approved displayed appearance; it
does not establish physical correctness of the first reference or validate HDR
differences that disappear when the renderer produces final RGBA8 pixels.
