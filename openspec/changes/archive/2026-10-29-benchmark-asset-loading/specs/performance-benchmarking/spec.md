## ADDED Requirements

### Requirement: Asset-loading suite protocol
The application SHALL provide an `asset-loading` suite that measures the Flight Helmet and Sponza atrium workloads in fresh worker processes. It SHALL default to two repetitions per workload, support existing workload and repetition filters, and require an optimized Release build with assertions disabled.

#### Scenario: Default loading suite runs
- **WHEN** `asset-loading` is selected without filters
- **THEN** the runner launches four fresh workers, two per asset workload
- **AND** each worker stops after the first initial-scene presentation is observed, with no warm-up or steady-state measurement phase

#### Scenario: One workload or repetition count is selected
- **WHEN** `--workload` selects either loading workload or `--repetitions` changes the repetition count
- **THEN** only the selected workload or requested repetition count changes, and each repetition still uses a fresh worker

#### Scenario: Loading suite timing overrides are supplied
- **WHEN** `--warmup` or `--measure` is supplied with `asset-loading`
- **THEN** preflight rejects the incompatible option before launching workers

#### Scenario: Loading suite uses a non-Release build
- **WHEN** the loading suite is requested from a Debug or assertion-enabled build
- **THEN** preflight reports that an optimized Release build is required and launches no worker

### Requirement: Loading timing boundaries and completion records
Each successful loading worker SHALL report seven monotonic nanosecond durations in a versioned completion record. Readiness SHALL mean the application observed presentation of the initial scene, not physical scanout or isolated GPU upload completion.

#### Scenario: Successful asset load completes
- **WHEN** a loading worker observes its first presentation of the initial scene
- **THEN** its schema-version-2 completion record reports `engine_initialization_ns`, `asset_registration_ns`, `asset_instantiation_ns`, `scene_setup_ns`, `presentation_ready_ns`, `asset_to_ready_ns`, and `engine_to_ready_ns`
- **AND** asset-to-ready is the sum of registration, instantiation, scene setup, and presentation readiness
- **AND** engine-to-ready includes engine construction and the pre-asset-loading frame

#### Scenario: Presentation readiness is measured
- **WHEN** the initial scene first becomes presented and the application observes that presentation
- **THEN** presentation readiness spans scene-end through immutable publication, incremental GPU upload, first presentation, and game-thread observation
- **AND** the record does not claim to isolate GPU upload or physical display scanout

#### Scenario: Loading telemetry is collected
- **WHEN** the loading suite runs
- **THEN** workers persist raw CPU wall-clock timing records as JSONL and completion metadata in the run's worker output
- **AND** no GPU queries or frame telemetry are enabled for this suite
- **AND** the FPS/TPS diagnostic overlay remains visible where supported

### Requirement: Loading asset preparation and cache identity
Asset preparation and checksum verification SHALL finish before any loading timing begins. Loading results SHALL identify the prepared assets and cache policy, and comparisons SHALL require compatible machine, driver, build, workload, protocol, asset identity, and timing boundaries.

#### Scenario: Prepared assets are missing or present
- **WHEN** a loading run starts without its verified prepared cache
- **THEN** the runner prepares the pinned assets before worker launch and timing
- **AND** when the cache is present it verifies checksums before worker launch

#### Scenario: Previous valid PNG cache is migrated
- **WHEN** preparation finds a valid cache for the known previous PNG profile
- **THEN** it verifies the cached sources and builds the DDS-profile result in a sibling staging directory
- **AND** it retains the old cache until the complete new cache has validated
- **AND** publication preserves a backup of the old cache

#### Scenario: Cache cannot be safely migrated
- **WHEN** the current cache is corrupt or a previous cache has an unknown profile or converter
- **THEN** preparation fails closed without replacing the existing cache

#### Scenario: Cache state is reported
- **WHEN** a loading worker runs after preflight verification
- **THEN** result metadata states that the worker uses a fresh process while operating-system file-cache state is uncontrolled
- **AND** the result makes no cold-cache claim and does not attempt to flush caches

#### Scenario: Loading baseline is compatible
- **WHEN** a baseline is selected for loading results
- **THEN** the runner compares only results with matching machine, driver, build class, workload, protocol, prepared asset identity, cache policy, and timing boundaries
- **AND** it reports independent repetition medians, range, median absolute deviation, and the existing bootstrap interval when at least two repetitions exist on each side

### Requirement: GPU-ready derived benchmark textures
The benchmark SHALL identify prepared DDS texture assets by profile, converter, and exact hashes, and use only required DDS runtime images.

#### Scenario: Current DDS profile is prepared
- **WHEN** the asset preparer converts the pinned PNG inputs for the benchmark
- **THEN** it writes one base-level-only BC3/DXT5 DDS for each of the 87 images using profile `engine-pbr-opaque-bc3-v1` and converter `krisp-benchmark-assets/3`
- **AND** it preserves image dimensions, material bindings, geometry, and the original PNG source files
- **AND** runtime glTF uses a required `MSFT_texture_dds` source without a PNG fallback
- **AND** `prepared.json` records the profile, converter version, and exact hashes of the derived runtime files

#### Scenario: Lossy preparation is described accurately
- **WHEN** the benchmark profile or its results are documented
- **THEN** the documentation identifies BC3 as lossy, including for normal maps
- **AND** it makes no asset-loading speed claim without profiling evidence

#### Scenario: New DDS assets are prepared offline
- **WHEN** local pinned source files are used to prepare derived DDS assets without downloading
- **THEN** Pillow 11.3.0 is the pinned conversion dependency
- **AND** that dependency is required only when deriving new runtime files, not when reusing a valid current cache

### Requirement: Bounded loading worker failures
Loading workers SHALL have workload-specific startup timeouts and a shutdown margin. A failed or cancelled load SHALL remain an explicit failure and SHALL NOT produce fabricated timing values.

#### Scenario: Worker exceeds its startup timeout
- **WHEN** Helmet readiness exceeds 120 seconds or Sponza readiness exceeds 300 seconds
- **THEN** the coordinator terminates and reaps the worker with a bounded termination grace period, records the timeout, and continues independent workloads

#### Scenario: Worker fails or is cancelled before readiness
- **WHEN** a worker crashes, exits without valid completion metadata, or is cancelled before readiness
- **THEN** the run records a failed or cancelled result without synthetic durations and exits nonzero after preserving completed results

### Requirement: Exclusive instantiation attribution
Loading workers SHALL record exclusive wall-clock durations for dependency reads, image decoding, document processing, mesh conversion, material construction, ECS adoption and an unattributed remainder. The sum SHALL equal model instantiation wall time. Reports SHALL distinguish these diagnostic boundaries from isolated GPU time or physical disk throughput.

#### Scenario: Nested work is observed
- **WHEN** one loading stage executes inside another
- **THEN** the parent excludes the nested stage's duration from its exclusive total
- **AND** raw monotonic spans and exclusive times are saved to `instantiation_trace.jsonl`

#### Scenario: Loading instrumentation changes
- **WHEN** an instrumented loading result is compared with a prior uninstrumented result
- **THEN** schema version 2 and instrumentation version 2 cause baseline compatibility to fail closed
