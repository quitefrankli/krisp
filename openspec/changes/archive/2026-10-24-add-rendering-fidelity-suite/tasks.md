# Tasks

## 1. Reusable exact-frame capture

- [x] 1.1 Add an application-neutral bounded capture request/result API with immutable frame identity, extent/format metadata, pixel payload, cancellation and completion; verify focused session tests reject wrong-frame completion and resolve cancellation/shutdown without hangs.
- [x] 1.2 Share the recording path's exact-frame rendezvous where appropriate and add explicit resource readiness/frozen-frame handling; verify fixed poses cannot be replaced by a newer snapshot or interpolated by wall-clock time, and run the existing recording/frame tests.
- [x] 1.3 Factor production presentation readback with format-aware RGBA8 conversion and fence completion; verify supported channel orders and failure for unsupported formats, and capture one real production frame with UI/diagnostic overlays disabled.
- [x] 1.4 Document capture ownership, readiness, synchronization, and failure behavior in `docs/GRAPHICS_ENGINE.md`; verify documentation matches the API and update `docs/PERFORMANCE.md` if ordinary rendering costs materially change.

## 2. Dedicated worker and unattended runner

- [x] 2.1 Add the `rendering_fidelity` internal application, C++ worker, Python coordinator, and Meson/launcher integration; verify its help and one selected fixture launch through `./run rendering_fidelity -- run --case CASE_ID` without altering stress commands or their Release policy.
- [x] 2.2 Define versioned case/capture manifests and machine-readable evidence with effective environment/settings, fixture hashes, source/dirty provenance, deadlines, and at least two fresh processes per case; verify runner tests exercise timeout, malformed/missing completion, continuing independent cases, and evidence retention.
- [x] 2.3 Implement readiness-based fixed-state capture and explicit nonpassing graphics/asset/configuration preflight errors; verify missing prerequisites do not silently skip cases or substitute content.
- [x] 2.4 Add `docs/RENDERING_FIDELITY.md` with current-setup requirements, Debug/Release profile separation, selection, run evidence paths, and failure statuses; verify commands against the runner interface.
- [x] 2.5 Isolate each run on one private Xvfb display without visible desktop windows or software-renderer provisioning; verify the real run selects the current hardware GPU, missing prerequisites fail without desktop fallback, and display processes are cleaned up after completion or failure.

## 3. Deterministic fixtures and textured scene

- [x] 3.1 Add `colour-presentation`, `pbr-factors`, and `textured-normals` definitions/assets with declared exposure variants and material/texture parameters; verify manifests enumerate all required captures and fresh-process outputs use identical fixed settings/state.
- [x] 3.2 Add `environment-lighting` and `point-shadows` cases with fixed environment input, multiple coloured lights, and shadowed/unshadowed variants; verify captures exercise distinct lighting configurations and contain no time-dependent scene state.
- [x] 3.3 Add `visibility-alpha` and `hierarchy-skinning` fixtures covering depth, sidedness, masking, blending, hierarchy composition and prescribed bind/animated poses; verify each variant is captured by the production pipeline at the specified snapshot.
- [x] 3.4 Expose the minimal shared prepared-asset verification/preparation interface and add the fixed Sponza `textured-environment` case; verify the existing prepared cache/recipe is reused, corrupt assets fail, and existing asset preparation tests pass if their tooling changes.
- [x] 3.5 Document each case, capture/pose identity, settings, fixture ownership, Sponza profile/licences, and explicit preparation command in `docs/RENDERING_FIDELITY.md`; verify the default inventory includes every case and capture in the spec.

## 4. Exact comparison and baseline lifecycle

- [x] 4.1 Implement full decoded RGBA/dimension comparison and fresh-process repeatability checks; verify focused tests cover identical pixels with different PNG encodings, a one-channel/one-pixel change, alpha-only changes, dimension changes, and repeatability disagreement even when one capture matches a baseline.
- [x] 4.2 Implement baseline profile compatibility and distinct aggregate statuses/nonzero outcomes; verify tests cover missing/corrupt baselines, changed GPU/driver/assets/settings/build class, source revision differences remaining comparable, and all-selected-captures success.
- [x] 4.3 Add approved baseline storage and a separate exact-digest promotion manifest plus explicit user-confirmed promotion; verify temporary-directory tests prove ordinary runs never modify baselines, initial/mismatching candidates can be proposed, incomplete/nondeterministic evidence cannot be promoted, stale candidate/destination changes fail, and publication cannot leave a partial approved set.
- [x] 4.4 Document user approval of specific evidence, agent prohibition on self-approval, environment changes, and recovery from stale promotion requests; verify the interface has no automatic or blanket baseline acceptance path.

## 5. Optional human comparison report

- [x] 5.1 Generate static local baseline/current/difference HTML with diagnostic changed-pixel counts/bounds, settings/status, repeatability evidence, and missing-image explanations; verify a sample report opens offline and clearly labels amplified differences.
- [x] 5.2 Wire `--report` to the same immutable capture/comparison results; verify tests show image verdicts and baseline files are unchanged with reporting enabled/disabled and report-write errors are distinct from comparison outcomes.
- [x] 5.3 Document report generation and review in `docs/RENDERING_FIDELITY.md`; verify the documented command yields side-by-side evidence for both a mismatch and an approval-required case.

## 6. Integration and initial reviewed baseline

- [x] 6.1 Register focused coordinator/comparator tests with Meson and a separately invokable GPU fidelity test entry; verify explicit GPU invocation fails clearly for missing prerequisites and routine CPU checks do not unexpectedly require graphics/assets/baselines.
- [x] 6.2 Build with `meson compile -C build/debug -j 6 sandbox` and the new fidelity target, then run `meson test -C build/debug -j 6` with concise GoogleTest output; verify supported existing capture/recording behavior remains intact. If dependency/build configuration changes require it, follow the repository's Conan/Meson setup sequence first.
- [x] 6.3 Run the full GPU suite on the current setup with at least two fresh captures per case and optional report; verify exact repeatability for every capture, fix nondeterminism without tolerances, and present the complete candidate/promotion manifests and visual evidence for user review. Without approved baselines, the expected outcome is approval-required, not success.
- [x] 6.4 After separate explicit user approval of the identified initial candidate set, promote only that set and rerun the GPU suite against it; verify all requested captures match exactly. Leave this task pending until that approval exists; implementation authorization alone does not approve baselines.

## Workflow follow-up

- Review the planning and implementation diff before archiving. Do not commit without consulting the user.
