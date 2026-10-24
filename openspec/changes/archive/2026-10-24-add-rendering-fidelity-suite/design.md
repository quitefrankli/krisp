# Design

## Context

See proposal.md for motivation and scope. `GraphicsEngineFrame` currently copies the display image after presentation and the diagnostic quad, before ImGui. The screenshot request is consumed from the debug UI and written to a timestamped path; readback assumes BGRA byte ordering. Recording separately associates a capture target with an immutable frame and synchronizes simulation and rendering. Ordinary frame publication is latest-wins.

`RuntimeOptions` already controls extent, fixed simulation scheduling, UI, and performance overlays. `RuntimeGraphicsInfo` records device, driver, extent, and validation state. Internal applications are registered in `applications/meson.build`; `./run` already handles named applications. The stress tool prepares and verifies pinned Sponza assets in a shared cache, but its measurement scheduling and Release-only policy are not fidelity requirements. Existing OpenSpec capabilities cover performance measurement, not pixel fidelity; historical benchmark spec details differ from current implementation and are outside this change.

## Goals / Non-Goals

**Goals:** Exercise production pipelines with a small test-facing surface, make exact comparison reproducible on the current setup, and keep captures, verdicts, reports, and baseline promotion separately reviewable.

**Non-Goals:** A second renderer, a fully offscreen Vulkan architecture, machine-wide environment provisioning, cross-driver identical output, analytical BRDF certification, tolerance-based assertions, or a new performance protocol. Ray tracing stays disabled.

## Decisions

### 1. Dedicated fidelity application and coordinator

Add `applications/rendering_fidelity/` with a C++ scene/capture worker and a Python coordinator/comparator/report tool. Integrate with the existing launcher as `./run rendering_fidelity -- run`, with optional `--case CASE_ID` and `--report`. Debug is the initial documented profile; Debug and Release results never share a baseline identity. Unlike stress benchmarks, fidelity is not restricted to optimized Release.

Each selected case uses at least two fresh sequential worker processes. Each worker loads the case, completes resource preparation, freezes the prescribed immutable scene state, captures its identified frame, writes evidence, and exits. Use bounded process and capture deadlines with orderly cancellation and termination fallback. Continue independent cases after failures and preserve their results.

Each run owns one private Xvfb display shared by those processes. Start through `xvfb-run` with a 640x480 24-bit screen and local-only transport, wait for server readiness, and clean up at exit. Require Xvfb/xauth; never fall back to the desktop display. The virtual display leaves the production window/swapchain path intact and uses the current hardware Vulkan GPU, which is verified in run metadata. Help and baseline operations do not start Xvfb. This replaces the original visible-window execution following the user's request to avoid disrupting their desktop; it does not introduce a software renderer.

Alternative: add another stress benchmark suite. Rejected because fidelity needs exact frame capture, repeatability, and approval policy, while benchmarks measure independent throughput and cadence. Keep engine code unaware of test cases and baseline policy.

### 2. Reusable capture at the production presentation boundary

Provide an opt-in, bounded capture request/result mechanism naming an immutable frame and returning its identity, extent, pixel format, canonical RGBA8 pixels, and success/failure. Complete readback only after the submission fence. Shutdown, resize, unavailable targets, and failed submissions must resolve requests explicitly rather than hang or capture an arbitrary newer frame.

Factor the reusable exact-frame synchronization from the recording path, preserving recording behavior. Freeze static fixtures and advance animated fixtures by prescribed fixed steps; suppress wall-clock interpolation for requested captures. Resource upload/composition readiness must be explicit, not an arbitrary sleep or a chosen number of warm-up seconds.

Use the existing presentation output after exposure, tone mapping, and display encoding. Disable diagnostic quads, editor decoration, particles unless explicitly defined by a case, UI, and performance overlays. Derive channel conversion from the effective Vulkan format; support only explicitly handled formats and fail on others. Channel swizzling and consistent image origin establish the storage contract; do not resample, colour-correct, quantize further, discard alpha, or otherwise normalize differences. Retain the debug screenshot UI as a consumer of the common readback where practical.

Alternative: compare video frames or drive the screenshot button. Rejected because video encoding and UI timing obscure exact pixel identity. A standalone offscreen renderer would omit the production presentation path and expand scope.

### 3. Versioned, deterministic case definitions

Use repository-owned small meshes, glTF fixtures, textures, and environment inputs, with explicit camera, transforms, resolution (initially 640x480), lighting, exposure, shadow settings, clear colour, and expected capture state. Record actual effective values and reject unexpected substitutions. Initial cases:

| Case | Exercised behavior |
| --- | --- |
| `colour-presentation` | Unlit factor and texture patches, sRGB handling, emissive/HDR values, fixed exposure variants through presentation |
| `pbr-factors` | Metallic/roughness sphere grid under fixed point lighting |
| `textured-normals` | Base colour, metallic/roughness, emissive textures, UVs and tangent-space normal orientation |
| `environment-lighting` | Fixed cubemap with diffuse/specular IBL and roughness variation |
| `point-shadows` | Fixed caster/receiver arrangement with multiple coloured lights and shadowed/unshadowed variants |
| `visibility-alpha` | Depth overlap, sidedness, alpha mask, and overlapping blended surfaces |
| `hierarchy-skinning` | Nested transforms and static/skinned equivalents, with prescribed bind and animated poses |
| `textured-environment` | One fixed Sponza atrium view with textures, normal maps, and multiple shadow-casting lights |

Named variants/poses are separate capture IDs within the case manifest; every expected capture is required. No random/time-dependent placement, animation time, camera movement, or unstable transparency order. Record hashes of actual fixture files and a canonical case configuration.

For Sponza reuse the existing pinned prepared asset recipe, verification, and licences. Expose only the needed preparation/verification entry points through a small shared tooling boundary if necessary; preserve existing stress commands, cache location, and recipe identity. Do not duplicate conversion logic or pull benchmark workload/measurement policy into fidelity. Workers consume verified local assets without network access. Missing assets produce actionable preparation instructions; no geometry substitute or automatic case skip. This baseline covers the declared prepared material profile, not unsupported source features.

### 4. Exact verdict and compatibility

Decode images to the capture contract and compare dimensions and every RGBA channel value. PNG compression and ancillary metadata have no bearing on equality. Use direct byte comparison; hashes identify evidence but are not a substitute for pixel comparison. No masks, tolerances, ignored borders, perceptual metrics, or numerical assertions that can override equality.

First compare all repeated fresh-process captures with one another. Any difference makes the case nondeterministic and nonpassing, even if one capture matches the baseline. Then compare every capture to the selected approved baseline.

The result schema distinguishes `pass`, `mismatch`, `nondeterministic`, `approval-required`, `incompatible`, and `error`. A run succeeds only when every requested case/capture passes; all other statuses return nonzero. Preserve separate reasons when more than one problem applies. Missing or corrupt captures/baselines never count as equality or cause a case to disappear.

Compatibility includes GPU/vendor/device and driver identity, OS/platform and relevant Vulkan identity, build class and validation state, effective extent/format/colour space and renderer settings, case/fixture version, prepared asset profile and hashes, and capture schema. Record source revision and dirty content fingerprint as provenance, but allow source/shader revisions to differ: comparing engine changes is the purpose. An environment/configuration mismatch requires a newly reviewed baseline profile, not a forced comparison or fallback profile. The initial suite uses the actual current setup without a software-renderer option.

### 5. Immutable approved baselines and scoped promotion

Store approved lossless PNGs and manifests under `test/rendering_fidelity/baselines/<profile-id>/`; store small fixtures beside case definitions. Profile IDs derive from canonical compatibility fields. Runs create unique evidence directories under application writable storage and never write approved baseline files. Preserve candidate captures, decoded-pixel/file digests, source/environment manifests, and comparison results for both successful and failed cases. No network resources are needed to read a report.

Provide a separate baseline proposal/promotion operation, with a manifest naming exact case/capture IDs, candidate digests, destination profile, existing baseline digests, and review evidence. Preparing this manifest does not approve it. Initial creation and every replacement require the user to explicitly approve that concrete manifest after evidence is available. An interactive user confirmation can authorize local promotion; an agent may execute promotion only after explicit conversation approval for the identified candidates. Do not provide an automatic update-on-failure, blanket acceptance, or an unattended flag that counts as user approval.

Before promotion recheck candidate hashes, repeatability completion, environment/profile identity, and the expected current baselines. Reject changed candidates, stale destination state, incomplete evidence, or nondeterministic captures. Stage a complete baseline set and publish atomically so failure cannot leave a partial approved set. Preserve approval scope and approved digests in the manifest. Human approval is a workflow boundary, not something a CLI flag or approval-looking file can independently prove; document this limitation clearly.

Alternative: overwrite snapshots when tests fail. Rejected because it lets a regression silently redefine correctness. Implementation work may generate candidate evidence but does not authorize creation of approved baselines.

### 6. Optional report over immutable run results

Always emit a machine-readable manifest/results file and concise console outcome. With `--report`, create static local HTML showing baseline/current/difference for each capture, changed pixel count and bounds, and environment/status details. Amplified differences are explicitly labelled diagnostics, never inputs to the verdict. Include repeatability differences and clear placeholders when a baseline/capture is unavailable. Report generation reads the same evidence and cannot recapture, promote baselines, or modify the result. A report I/O failure is reported separately and makes the requested command incomplete without changing recorded image verdicts.

## Risks / Trade-offs

- Exact rendering may expose uninitialized resources or timing-dependent state -> test repeatability first, preserve every capture, fix the cause, and never hide it with tolerance or baseline churn.
- GPU or driver updates invalidate approved profiles -> retain old baselines and require reviewed promotion for the current profile.
- Final RGBA8 output can hide sub-quantization HDR errors -> document that the contract verifies final displayed pixels; HDR intermediate assertions are outside this change.
- Large asset preparation is expensive -> share verified existing prepared assets, keep focused cases selectable, and report preparation failures distinctly.
- Capture synchronization could affect normal scheduling -> activate only for explicit capture sessions, preserve recording semantics, and run relevant existing recording/frame tests. Update `docs/PERFORMANCE.md` if implementation materially changes ordinary rendering costs.
- An approved reference can contain a visual bug -> require human review of initial evidence; exact comparison detects changes, not physical correctness of the first reference.

## Migration Plan

Add the application and opt-in capture interface without changing ordinary consumer scheduling or benchmark policy. Integrate focused runner/comparator tests with Meson and provide a separately named GPU fidelity test entry so routine CPU checks do not unexpectedly require graphics/assets/baselines. The explicit GPU entry must fail if its prerequisites are missing rather than skip.

Run all cases at least twice across fresh processes on the current setup, collect the optional report, and present the exact candidate set for initial user approval. Until approved, report `approval-required`; do not fabricate a green initial run. Publish approved baselines only after that separate approval, then verify exact matching. Rollback removes or disables the fidelity entry points and restores any replaced baseline set from version control; no save-data or asset-format migration is required.
