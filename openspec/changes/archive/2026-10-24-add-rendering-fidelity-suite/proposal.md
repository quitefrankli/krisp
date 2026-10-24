# Proposal

## Why

Rendering-related unit tests verify imported data and CPU-side mechanisms, but do not verify the pixels produced by the production Vulkan renderer. A deterministic fidelity suite will catch rendering regressions automatically and provide optional visual evidence for reviewing intentional changes.

## What Changes

- Add a dedicated fidelity runner using the current machine's GPU and driver, separate from performance benchmarking.
- Render controlled fixtures for colour/presentation, PBR factors, textures and normal maps, direct and environment lighting, shadows, depth/sidedness/transparency, transforms, and skinning; include one fixed larger textured Sponza view.
- Expose application-neutral capture of an identified immutable frame after presentation processing and before UI, with explicit completion and failure reporting.
- Require exact equality of every decoded RGBA pixel and image dimensions. No tolerances, masks, perceptual verdicts, or image normalization that conceals differences.
- Verify repeatability across fresh processes and record environment, effective settings, fixture/asset identity, and source provenance.
- Keep approved baselines immutable during runs. Initial creation and replacement require explicit user approval of specific candidate evidence.
- Optionally produce a local HTML report showing baseline, candidate, and difference images; reporting cannot change the automated verdict.

## Capabilities

### New Capabilities

- `rendering-fidelity`: Deterministic rendering cases, exact image verdicts, approved baseline lifecycle, environment compatibility, and optional visual comparisons.

### Modified Capabilities

None. Performance benchmarking and runtime observation retain their existing contracts; fidelity has its own execution and result policy.

## Impact

- Engine frame scheduling/capture boundaries, currently implemented by screenshot and recording paths.
- A dedicated internal application under `applications/`, its runner, Meson targets, and focused automated tests.
- Reuse of verified prepared textured assets through a shared preparation interface; no dependency from engine code to benchmark policy.
- New fidelity documentation, case definitions, baseline manifests/images, and run artifacts.
- No pinned software renderer, cross-device fidelity guarantee, ray tracing, performance benchmark protocol change, or automatic baseline approval.
