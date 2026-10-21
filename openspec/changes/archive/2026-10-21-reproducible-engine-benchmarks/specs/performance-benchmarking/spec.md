# Spec Delta

## Purpose

Provide unattended, evidence-backed comparisons of engine rendering throughput and 60 Hz frame pacing on the same machine through the stress_test application.

## ADDED Requirements

### Requirement: One-command unattended suite
The application SHALL run a named suite from one command without interactive setup. Omitting `--suite` SHALL select `standard`. Each repetition SHALL use a fresh process, bounded timeouts and a recorded measurement lifecycle. Failures SHALL preserve completed results.

#### Scenario: Default suite completes
- **WHEN** the suite is run without `--suite` on a supported machine with writable application data storage and prepared assets
- **THEN** all cases and repetitions execute sequentially without manual interaction and produce a machine-readable result and readable report
- **AND** it runs `static-objects-500`, `moving-transforms-500`, `point-shadows-500`, `textured-objects-16`, and `textured-environment-atrium` in throughput and paced modes, with two repetitions, 2 seconds of warm-up and 5 seconds of measurement per worker
- **AND** its schedule contains 20 workers and 140 seconds of configured warm-up and measurement
- **AND** Sponza startup is hardware-dependent, so the default suite has no five-minute completion guarantee; first-time asset preparation is also outside the configured timed-work total

#### Scenario: Smoke suite checks correctness
- **WHEN** the `smoke` suite is requested
- **THEN** one diagnostic case, one Flight Helmet case, and the fixed Sponza atrium view run in both modes, with one repetition, 0.2 seconds of warm-up and 0.5 seconds of measurement
- **AND** results are interpreted as correctness checks, not performance evidence

#### Scenario: Worker lifecycle is unattended
- **WHEN** a suite runs
- **THEN** the application performs preflight, waits for readiness, warms up, measures, saves results and exits without interactive setup

#### Scenario: Asset loading is outside current benchmark metrics
- **WHEN** a standard worker measures a workload
- **THEN** asset preparation, loading and GPU upload are excluded from measured statistics
- **AND** asset-loading performance measurement remains a separate future benchmark

#### Scenario: A child hangs or fails
- **WHEN** a repetition times out, crashes, or exits without a valid completion record
- **THEN** the runner terminates that child if necessary, records the failure, continues independent cases, and exits nonzero after writing available results

### Requirement: Optional workload selection
The public `run` command SHALL accept an optional `--workload CASE_ID` filter, defaulting to all cases in the selected suite. A selected case SHALL retain its suite-defined modes, repetitions, timings, metrics, effective settings and per-case baseline identity. Invalid selections SHALL be rejected before asset preparation or worker launch.

#### Scenario: Standard case is selected
- **WHEN** `--workload point-shadows-500` is supplied to the default `standard` suite
- **THEN** only `point-shadows-500` runs in throughput and paced modes with two repetitions, 2 seconds of warm-up, and 5 seconds of measurement
- **AND** accepted standard IDs include `static-objects-500`, `moving-transforms-500`, `point-shadows-500`, `textured-objects-16`, `textured-environment-atrium`, `textured-environment-corridor`, and `textured-environment-material-closeup`
- **AND** four fresh workers launch
- **AND** its metrics, effective settings, and per-case baseline compatibility identity match the same case in an unfiltered standard run

#### Scenario: Smoke case is selected
- **WHEN** `static-objects-100`, `textured-objects-1`, or `textured-environment-atrium` is selected with `--suite smoke`
- **THEN** only that case runs in both modes with one repetition, 0.2 seconds of warm-up, and 0.5 seconds of measurement

#### Scenario: Workload is unknown or belongs only to another suite
- **WHEN** an unknown workload ID or a case ID that is valid only in the other suite is supplied
- **THEN** preflight rejects the command before asset preparation and worker launch

### Requirement: Selectable Sponza views
Standard SHALL include `textured-environment-atrium` in its default protocol and accept `textured-environment-corridor` and `textured-environment-material-closeup` as optional workloads. All SHALL use pinned full-resolution assets and fixed views; each selected view SHALL retain standard settings and per-view evidence. Sponza startup is variable; no five-minute completion guarantee applies.

#### Scenario: A standard Sponza view is selected
- **WHEN** one of the three Sponza view IDs is selected with `--suite standard`
- **THEN** that view runs in both modes with two repetitions, 2 seconds of warm-up, and 5 seconds of measurement
- **AND** four fresh workers produce the usual per-view metadata, raw records, report, and baseline identity
- **AND** the default five-workload schedule remains unchanged

#### Scenario: Sponza repetitions are reduced
- **WHEN** a standard Sponza view is selected with `--repetitions 1`
- **THEN** two workers run, one per mode, with weaker comparison evidence and no directional conclusion under the two-repetition minimum

### Requirement: Versioned controlled scenarios
Standard and smoke suites SHALL use versioned scenarios with explicit, controlled workload and rendering settings.

#### Scenario: Object count changes
- **WHEN** a static-object case runs at two prescribed counts
- **THEN** only the documented object workload changes while camera, lighting, and graphics settings remain identical

#### Scenario: Scenario configuration is recorded
- **WHEN** a scenario is defined
- **THEN** its geometry, material sharing, placements, camera, lighting, resolution, graphics settings and deterministic motion are specified

#### Scenario: Motion repeats
- **WHEN** the same moving-transform scenario reaches the same simulation tick in separate repetitions
- **THEN** it produces the same requested transforms independently of wall-clock speed

### Requirement: Pinned local benchmark assets
The application SHALL provide manual preparation of pinned Intel Sponza Base Scene and Khronos Flight Helmet assets with verified checksums, preserved licences and attribution, and a recorded recipe. Workers SHALL use verified local files without network access.

#### Scenario: Pinned sources are prepared
- **WHEN** preparation receives downloaded sources or local pinned inputs supplied through `--source-directory`
- **THEN** it verifies their checksums, preserves licences and attribution, and records the preparation recipe

### Requirement: Automatic shared asset preparation
Manual preparation and runs SHALL use `build/benchmark-assets`, shared by Debug and Release. The public commands SHALL NOT accept an asset-cache path override. Runs SHALL prepare a missing cache before workers and warm-up, verify and reuse existing caches, and reject corrupt or incompatible caches without overwriting them.

#### Scenario: Prepared assets are available
- **WHEN** a suite starts with a verified matching asset cache
- **THEN** it reuses those assets without downloads or conversion and excludes asset loading and GPU upload from steady-state measurements

#### Scenario: Asset cache is missing
- **WHEN** a suite starts and its cache directory is missing
- **THEN** the runner prepares pinned assets before launching workers or starting warm-up, using downloaded sources by default or pinned local inputs supplied through `--source-directory`

#### Scenario: Asset cache is altered or incompatible
- **WHEN** an existing prepared asset or dependency is missing, modified, or does not match the pinned profile
- **THEN** preflight fails with an actionable diagnostic and does not overwrite the cache, substitute geometry, or silently skip textured cases

### Requirement: Default result directory
Each public run SHALL create a unique result directory under `$XDG_DATA_HOME/krisp-engine/stress-test`, falling back to `~/.local/share/krisp-engine/stress-test` when `XDG_DATA_HOME` is unset. The public run command SHALL NOT accept an output-directory override.

#### Scenario: A run starts without an output path
- **WHEN** a suite runs
- **THEN** the coordinator creates a unique result directory under the default application data path and preserves previous runs

#### Scenario: A user requests an output override
- **WHEN** `--output` is supplied to the public `run` command
- **THEN** argument parsing rejects the unsupported option before launching workers

### Requirement: Explicit material compatibility profiles
Prepared assets SHALL preserve intended geometry and supported base-colour, normal and metallic/roughness textures. Any material adaptation SHALL be explicit, versioned and included in result identity. Unsupported features outside the preparation recipe SHALL fail preparation. Reports SHALL identify the derived profile rather than claim full source-material fidelity.

#### Scenario: Flight Helmet requires unsupported materials
- **WHEN** its pinned source contains transmission or separate occlusion bindings unsupported by the engine
- **THEN** a documented opaque PBR profile records the removed bindings and material changes while retaining geometry and supported texture maps
- **AND** the importer is not permitted to silently discard features during benchmark loading

#### Scenario: Pinned Sponza emissive rounding is normalised
- **WHEN** the pinned Sponza light_bulb material has emissiveFactor [1.0, 0.99999994, 1.00000012]
- **THEN** preparation changes only the last component to 1.0 and records that exact adjustment
- **AND** the original source is preserved and any other out-of-range material value fails preparation

### Requirement: Asset-backed scenario identity
Textured-object scenarios SHALL declare model-instance counts separately from actual mesh/renderable and texture counts. Environment scenarios SHALL declare fixed view and lighting definitions. Asset version, prepared content, material profile, texture dimensions and view identity SHALL participate in baseline compatibility; per-machine automatic asset simplification SHALL NOT occur.

#### Scenario: Prepared texture resolution changes
- **WHEN** candidate and baseline use different derived textures, preparation profiles or environment views
- **THEN** they are reported as incompatible cases rather than as engine performance changes

### Requirement: Throughput and 60 Hz pacing modes
Every standard scenario SHALL run in throughput and paced modes. Throughput SHALL request uncapped rendering with simulation targeting 60 Hz; paced mode SHALL target 60 Hz for both. Both SHALL retain independent execution and report achieved simulation and graphics rates. Unsupported throughput presentation settings SHALL be reported as a constrained result.

#### Scenario: Simulation cannot sustain its target
- **WHEN** throughput rendering or simulation work prevents the simulation from achieving 60 Hz
- **THEN** results show the achieved tick rate and target shortfall instead of treating repeated snapshots as additional simulation work

#### Scenario: Presentation constrains throughput
- **WHEN** only display-synchronised presentation is available
- **THEN** results identify the effective presentation constraint and do not claim unrestricted engine throughput

### Requirement: Visible benchmark performance rates
Benchmark workers SHALL enable the FPS/TPS overlay; ordinary consumers SHALL
retain its default-off setting. The overlay setting SHALL be recorded and affect
compatibility. Overlay work SHALL remain in measured costs; displayed rates are
diagnostic only.

#### Scenario: Benchmark overlay is shown and recorded
- **WHEN** a benchmark worker loads or measures a scenario
- **THEN** FPS and TPS are displayed during warm-up and measurement, and during
  loading where supported
- **AND** editor panels, application UI, and input/capture controls remain disabled
- **AND** the result records `effective_settings.performance_overlay=true`
- **AND** overlay update and draw work remains included in measured timings
- **AND** raw benchmark observations remain the source of reported metrics

#### Scenario: Overlay setting differs between runs
- **WHEN** candidate and baseline results use different effective performance
  overlay settings
- **THEN** the cases are reported as incompatible

#### Scenario: Routine protocol retains its schedule
- **WHEN** the default standard suite is run
- **THEN** it retains 20 workers and 140 seconds of configured warm-up and
  measurement with the performance overlay enabled throughout each worker's
  warm-up and measurement

### Requirement: Explicit measurement lifecycle
A repetition SHALL exclude startup and warm-up from measured statistics, record their durations, and enforce a configured measurement window. Samples SHALL be assigned by their original execution phase rather than delayed collection time. Resize, focus loss, minimisation, telemetry loss, or runtime configuration changes during measurement SHALL invalidate the affected repetition with a reason.

#### Scenario: GPU results arrive after measurement ends
- **WHEN** GPU samples for measured submissions arrive during final collection
- **THEN** they remain part of the measured repetition while warm-up and teardown samples remain excluded

#### Scenario: Window is resized
- **WHEN** the rendered extent changes during measurement
- **THEN** the repetition is marked invalid and excluded from performance conclusions

### Requirement: Raw evidence and labelled summaries
Results SHALL include raw tick, frame, GPU, wait, and snapshot records with units and validity flags, plus per-repetition counts, achieved rates, median, p95, and p99 durations where available. Paced reports SHALL include deadline lateness, missed 60 Hz deadlines under a recorded tolerance, and repeated or skipped snapshots. Present-call cadence SHALL be labelled as a CPU-side measurement.

#### Scenario: GPU timings are unavailable
- **WHEN** timestamp support is absent or required samples are missing
- **THEN** GPU metrics are explicitly unavailable and the report retains valid CPU metrics without substituting zero or CPU elapsed time

#### Scenario: Graphics resubmits a snapshot
- **WHEN** several successful graphics submissions reference one simulation snapshot
- **THEN** graphics throughput and repeated-snapshot counts increase while achieved simulation throughput does not

### Requirement: Provenance and compatibility
Results SHALL record schema and scenario versions, source revision and dirty-state identity, build configuration, validation state, OS, CPU and GPU identity, driver, and requested and effective runtime settings. Comparisons SHALL require matching machine identity, workload, measurement protocol, build class, and effective settings; source revisions SHALL be allowed to differ.

#### Scenario: Settings do not match
- **WHEN** a baseline differs in resolution, presentation mode, driver, scenario version, or build class
- **THEN** the report identifies the incompatibility and does not produce a regression conclusion for that case

#### Scenario: A dirty tree was measured
- **WHEN** source changes are uncommitted
- **THEN** the result identifies that state and records a source-content fingerprint so the run is not attributed solely to its commit

### Requirement: Explicit baseline and variation-aware comparison
Baseline selection SHALL be explicit and SHALL NOT overwrite an existing baseline automatically. Comparisons SHALL use independent repetition summaries, show absolute and relative changes with between-repetition variation, and mark uncertain changes inconclusive. Initial comparisons SHALL be advisory rather than an automatic performance pass/fail gate.

#### Scenario: Change is indistinguishable from variation
- **WHEN** compatible repetition results do not support a directional difference under the documented comparison method
- **THEN** the report labels the comparison inconclusive and retains its measured differences and variation

#### Scenario: No baseline is supplied
- **WHEN** a suite is run without a baseline
- **THEN** it produces a complete standalone report without selecting or replacing a baseline

### Requirement: Honest preflight and result status
Preflight SHALL check graphics availability, output writability, requested settings, and measurement capabilities without changing machine-wide power, driver, or display configuration. Reports SHALL distinguish complete, partial, invalid, and failed cases. A valid CPU-only run SHALL be partial when GPU timing is unsupported; missing samples despite supported measurement SHALL invalidate the affected metric.

#### Scenario: Debug build is used
- **WHEN** the runner measures a Debug or validation-enabled build
- **THEN** the report prominently identifies that configuration and does not compare it with an optimised validation-disabled baseline

#### Scenario: Preflight cannot start graphics
- **WHEN** no usable graphics device or window system is available
- **THEN** the run exits nonzero with a diagnostic result rather than requiring interactive recovery

Approved tangent correction: the pinned Sponza source contains 39 tangents parallel to their normals (37 in decals_1st_floor, two in lamps_2ndfloor_02). Preparation replaces only these exact pinned tangent vectors with the approved deterministic perpendicular unit vectors, preserving handedness and all positions, normals, indices and textures. The lock records every before/after value and derived buffer checksum; any source mismatch fails preparation.

### Requirement: Release benchmark configuration
Standard performance benchmark runs SHALL require an optimized Release build with assertions disabled and SHALL reject unsupported or unknown build configurations before launching any worker. Debug builds MAY run smoke correctness checks, which SHALL NOT be interpreted as performance benchmarks.

#### Scenario: Debug standard benchmark is rejected
- **WHEN** a standard benchmark is requested from a Debug build
- **THEN** preflight fails with a Release build instruction and launches no workers
