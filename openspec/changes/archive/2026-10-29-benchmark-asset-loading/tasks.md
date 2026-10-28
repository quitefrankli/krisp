# Tasks

## 1. Engine readiness timestamp

- [x] 1.1 Add a queryable monotonic timestamp for the engine's first observed presentation of its initial scene, set once before readiness is reported; verify unavailable-before/present-after behavior and that later presentations do not replace it.
- [x] 1.2 Document the timestamp's clock domain and application-neutral presentation boundary in `docs/RUNTIME_OBSERVATION.md`; verify the wording distinguishes application observation from physical scanout.

## 2. Loading worker measurements

- [x] 2.1 Add loading-worker timers around engine construction, `load_asset`, `instantiate_model`, remaining scene setup, and presentation readiness; verify the seven durations and their sums against deterministic worker fixtures or focused lifecycle tests.
- [x] 2.2 Write schema-version-2 completion metadata and raw monotonic CPU timing JSONL for successful loading workers; verify failed or cancelled workers have no fabricated duration values.
- [x] 2.3 Keep loading workers free of warm-up, steady-state measurement, GPU queries, and frame telemetry while retaining the FPS/TPS overlay; verify loading worker configuration and its recorded settings.

## 3. Asset-loading suite protocol

- [x] 3.1 Add the Release-only `asset-loading` suite with two default repetitions for Flight Helmet and Sponza atrium, plus `--workload` and `--repetitions` filtering; verify the default produces four fresh worker launches and filtered schedules match their requested counts.
- [x] 3.2 Reject `--warmup` and `--measure` for `asset-loading` before worker launch and reject Debug or assertion-enabled builds; verify both preflight failures launch no workers.
- [x] 3.3 Reuse preflight asset preparation and checksum verification, record fresh-process and uncontrolled OS-cache policy in loading compatibility metadata, and apply workload-specific 120/300-second startup limits with shutdown margin; verify cache failures and worker timeouts preserve completed results and remain nonzero failures.

## 4. Results, comparisons, and documentation

- [x] 4.1 Add loading-specific reports and baseline compatibility checks for machine, driver, build, workload, protocol, asset identity, cache policy, and timing boundaries; verify incompatible baselines fail closed and compatible repetitions report median, range, MAD, and the existing bootstrap interval.
- [x] 4.2 Document the new suite, CLI options, record schema, timing boundaries, cache policy, and comparison limits in `docs/benchmarks/BENCHMARKS.md` and `docs/PERFORMANCE.md`; verify the documented defaults and examples match the runner.
- [x] 4.3 Run the four-worker asset-loading suite on Debug GPU and inspect both asset results and their provenance; verify Debug output can be compared against a compatible baseline, Release-only preflight launches no workers, and the existing smoke suite passes. Treat Debug durations as correctness diagnostics; do not claim Release performance measurements.

## 5. Instantiation attribution

- [x] 5.1 Add opt-in generic nested resource-loading spans with exclusive times and CPU-worker propagation; verify nested attribution and real asset loading callbacks.
- [x] 5.2 Record schema-version-2 loading metadata, exclusive stage totals, unattributed remainder and raw spans; validate exact sums, reporting, and instrumentation compatibility.
- [x] 5.3 Verify both pinned assets with GPU Debug correctness runs and inspect attribution; retain the Release gate and distinguish Debug diagnostics from the user's Release measurements.

## 6. Derived benchmark texture preparation

- [x] 6.1 Add profile `engine-pbr-opaque-bc3-v1` and converter `krisp-benchmark-assets/3` to produce one base-level-only BC3/DXT5 DDS from each of the 87 pinned PNGs; preserve dimensions, material bindings, geometry, and source PNGs, emit required `MSFT_texture_dds` references without PNG fallback, and record profile, converter, and exact runtime hashes.
- [x] 6.2 Add pinned Pillow 11.3.0 preparation dependency and safe migration from the known previous PNG cache: verify cached sources, prepare and validate in sibling staging, retain the old cache until publish, preserve its backup, and reject corrupt current or unknown old caches without overwriting them.
- [x] 6.3 Verify all derived assets and material bindings, checksum locking and baseline identity, offline preparation with the pinned converter, valid-cache reuse without Pillow, automatic missing-cache preparation, and migration/fail-closed behavior; document BC3 lossiness including normal maps and make no speed claim before profiling the prepared assets.
