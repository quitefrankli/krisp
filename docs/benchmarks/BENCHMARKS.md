# Performance benchmark

The performance mode has commands for asset preparation and benchmark execution:

```sh
./run benchmarks -- performance prepare-assets
./run benchmarks -- performance run
./run benchmarks -- performance run --suite smoke
```

`run` automatically prepares the pinned Flight Helmet and Sponza assets when its cache is missing, before starting workers or warm-up. Preparation downloads pinned sources unless local inputs are supplied with `--source-directory DIR`. A present cache is verified and reused; a corrupt or incompatible cache fails with a diagnostic. `prepare-assets` remains available for manual preparation and uses the same cache. The asset cache is shared at `build/benchmark-assets`.

Preparation profile `engine-pbr-opaque-bc3-normal-mips-v1` converts the 87 pinned
source PNGs to lossy BC3/DXT5 DDS runtime textures, with complete vector-filtered
mip chains for normal maps and a base level for other maps. A valid cache from
either previous PNG or base-only BC3 profile is migrated from its verified
sources before publication; the old cache is retained as a backup.
See [benchmark asset preparation](BENCHMARK_ASSETS.md)
for migration rules, cache identity, and the optional Pillow requirement for
new preparation.

## Build for benchmarking

Every performance suite, including `smoke`, requires an optimized Release build with assertions disabled. Debug execution is rejected before workers launch. From the repository root, configure a Release directory (omit `--reconfigure` on first setup):

```sh
meson setup build/release --reconfigure --native-file build/conan/conan_meson_native.ini --buildtype=release -Db_ndebug=true
meson compile -C build/release -j 6 benchmarks
build/release/applications/benchmarks/benchmarks performance run
```

The repository launcher can build and run it in one command:

```sh
./run benchmarks -- performance run
```

For offline automatic preparation, pass `--source-directory DIR` to `run` with the pinned source files in the layout described in [benchmark asset preparation](BENCHMARK_ASSETS.md). If dependencies have not been installed, first run `conan install . -pr=conan_clang_profile --build=missing`; the dependencies remain Release while Meson selects the engine configuration.

Use the repository launcher for all benchmark runs; it builds and selects the optimized Release configuration by default. The recorded effective settings and display setup must match between compared runs.

Every benchmark worker enables the engine performance overlay. It displays live
FPS and TPS during loading where the loading screen supports it, warm-up, and
measurement. The benchmark overlay setting is recorded as an effective setting
and participates in baseline compatibility. These displayed rates are
diagnostic; persisted benchmark records remain the source for reported metrics.

## Suites

The default `standard` suite runs `moving-transforms-500` and `point-shadows-500` in throughput mode, and `textured-environment-atrium-shadows` (Sponza) in both throughput and paced modes. Each workload and mode has two fresh processes, for eight launches total. It warms up each worker for 2 seconds and measures for 10 seconds, for 96 seconds of configured timed work. FPS/TPS overlay updates and drawing run during warm-up and measurement and are included in the measured costs. The full-resolution Sponza scene has longer, hardware-dependent startup, so the standard suite has no under-five-minute completion guarantee. First-time asset preparation is also outside the configured timed-work total and can take substantially longer.

The optional standard workload `textured-environment-atrium-shadows-environment`
adds the engine's default skybox and image-based environment lighting to the
one-light shadowed atrium. It provides diffuse irradiance and environment
reflections alongside point-light shadows. The environment is a global
approximation: it does not compute interior visibility or bounced light.
Open it with the full editor UI, camera controls, and normal user exit:

```sh
./run benchmarks --release -- performance run --suite standard --workload textured-environment-atrium-shadows-environment --interactive
```

Omit `--interactive` for the normal throughput and paced measurements. This
workload has its own baseline identity and does not change the default suite.

The optional `textured-environment-atrium-shadows-4-lights` and
`textured-environment-atrium-shadows-8-lights` workloads are available in both
`standard` and `smoke`. The default suite continues to use the original
one-shadow-light atrium workload. In standard, each selected workload runs in
both modes with two repetitions, 2 seconds of warm-up, and 10 seconds of
measurement. All variants use fixed light definitions; the multi-light
variants use prefixes of the same nested eight-position list, with the
original light position first and deterministic offsets for the remaining
positions. They split the existing total light intensity of 800 equally
across the selected lights. This controls aggregate intensity as a parameter,
but does not guarantee equal perceived brightness. Every light casts shadows
using a 1024-by-1024 cubemap and the renderer's current 16-tap PCF filter.
Workload identity records the actual per-light definitions and light count,
so each variant has its own baseline identity. The unshadowed atrium remains
the one-unshadowed-light reference, and asset loading is unchanged.

For focused standard runs, use:

```sh
./run benchmarks -- performance run --workload textured-environment-atrium-shadows-4-lights
./run benchmarks -- performance run --workload textured-environment-atrium-shadows-8-lights
```

Add `--interactive` to inspect a selected scene manually:

```sh
./run benchmarks --release -- performance run --suite standard --workload textured-environment-material-closeup --interactive
```

This launches the selected workload once in the regular editor mode, with the
full editor UI for lighting, materials, and objects, and paces it at 60 Hz.
Camera input uses the editor's existing controls; F1 toggles the regular FPS
display. Escape or closing the window exits normally. There is no automatic
measurement timeout or repetition;
warm-up, measurement, and repetition arguments are ignored. The session records
its completion without collecting benchmark samples or comparing baselines.
Interactive mode requires `--workload` from the standard or smoke suite.

Smoke runs the `static-objects-100`, `textured-objects-1`, and `textured-environment-atrium` cases in both modes. It uses one repetition, 0.2 seconds of warm-up, and 0.5 seconds of measurement. The shadowed atrium and its four-light and eight-light variants can also be selected explicitly in smoke without changing the default workload set. Smoke is a short Release lifecycle check; its measurements are too brief to serve as performance evidence. The suite name, timings, repetitions, workload, effective settings, build class, machine, selected GPU, and driver participate in compatibility, so smoke cannot be compared with `standard`.

The `asset-loading` suite measures `textured-objects-1` (Flight Helmet) and
`textured-environment-atrium` (Sponza) in separate fresh processes. It defaults
to two repetitions per workload, for four worker launches. Each worker exits
after the application observes the initial scene's first presentation; there
is no warm-up or steady-state measurement phase. Use a Release build with
assertions disabled. The suite rejects `--warmup` and `--measure`, while
`--workload` and `--repetitions` select the requested workloads and repetitions.

```sh
./run benchmarks -- performance run --suite asset-loading
./run benchmarks -- performance run --suite asset-loading --workload textured-objects-1 --repetitions 1
```

Preparation and checksum verification happen before workers start and before
timing. Each worker has a 120-second startup limit for Flight Helmet and a
300-second limit for Sponza, followed by the normal shutdown margin. A timeout,
crash, or cancellation before readiness is recorded as a failure without
invented durations; completed workers remain available in the result.

Loading completion metadata uses schema version 2 and reports seven monotonic
nanosecond durations: engine initialization, asset registration, asset
instantiation, scene setup, presentation readiness, asset-to-ready, and
engine-to-ready. Asset registration covers `load_asset`; instantiation covers
the `instantiate_model` call, including materialization, decoding, and ECS
adoption; scene setup covers application attachment, lighting, and camera
configuration. Presentation readiness spans scene-end through immutable
publication, incremental GPU upload, first presentation, and game-thread
observation. It is an end-to-end CPU wall-clock interval, not an isolated GPU
upload measurement or a physical scanout time. Successful loading workers
write raw timing records as JSONL and do not enable GPU queries or frame
telemetry. The FPS/TPS overlay remains available as a live diagnostic.

Each repetition starts a fresh process, but OS file-cache state is uncontrolled:
asset preparation and verification may have read the files before launch. The
runner does not flush caches and results make no cold-cache claim. Loading
baselines require matching machine, driver, build class, workload, protocol,
prepared asset identity, cache policy, and timing boundaries. Reports retain
per-repetition values, median, range, median absolute deviation, and the usual
independent-repetition bootstrap interval. The loading suite does not change
the `standard` or `smoke` rendering protocols.

Run the standard protocol with:

```sh
./run benchmarks -- performance run
```

Use `--workload CASE_ID` to run one existing case from the selected suite. By default, every case runs. For example, this selects only `point-shadows-500` in throughput mode, with two repetitions and a 2-second warm-up plus 10-second measurement (two worker launches):

```sh
./run benchmarks -- performance run --workload point-shadows-500
```

Standard accepts `moving-transforms-500`, `point-shadows-500`, `textured-environment-atrium`, `textured-environment-atrium-shadows`, `textured-environment-atrium-shadows-environment`, `textured-environment-atrium-shadows-4-lights`, `textured-environment-atrium-shadows-8-lights`, `textured-environment-corridor`, and `textured-environment-material-closeup`. Smoke accepts `static-objects-100`, `textured-objects-1`, `textured-environment-atrium`, `textured-environment-atrium-shadows`, `textured-environment-atrium-shadows-4-lights`, and `textured-environment-atrium-shadows-8-lights`. The Sponza views use the pinned full-resolution assets and fixed view definitions; each selected view retains normal per-view metadata, raw records, reports, and baseline identity. The shadowed atrium workloads use fixed point-light definitions, 1024-by-1024 cubemaps, and the renderer's current 16-tap PCF filter. The unshadowed atrium view remains selectable in standard and smoke; the corridor and material-closeup views remain unshadowed optional standard workloads. Shadowed and unshadowed atrium runs have distinct workload identities, and the recorded actual light definitions and count also participate in baseline compatibility. The filter selects cases only; it does not change case metrics, effective settings, or per-case baseline compatibility. Unknown cases and valid cases that belong only to the other suite are rejected during preflight before workers start or assets are downloaded/prepared.

The shadowed atrium Sponza view is part of the default standard schedule. The unshadowed atrium, corridor, and material-closeup views are optional standard workloads, each available in throughput and paced modes. Focused shadowed and unshadowed atrium runs use the same standard settings for a controlled workload comparison:

```sh
./run benchmarks -- performance run --workload textured-environment-atrium-shadows
./run benchmarks -- performance run --workload textured-environment-atrium
```

Each selected standard Sponza view runs in both modes with two repetitions, 2 seconds of warm-up, and 10 seconds of measurement (four fresh processes per view). The default suite has four workload/mode combinations, eight workers, and 96 seconds of configured timed work. To use one repetition and two worker launches for a focused view, pass `--repetitions 1`; this is faster but provides less evidence for comparison and cannot support a directional comparison under the minimum two repetitions per side. Keep machine, build, display setup, and timing settings the same across the two focused commands; their distinct workload identities mean they retain separate baselines rather than being a direct baseline comparison.

The coordinator prints the estimated timed-work duration and progress. Cases run sequentially in a deterministic shuffled order for each repetition round. Every repetition starts a fresh worker. A supported graphics session is required. When the default asset cache is absent, the runner prepares it before workers or warm-up; first-time preparation is outside the configured timed-work estimate. Existing caches must pass integrity checks; altered or incompatible caches fail preflight rather than removing textured workloads. Startup and final collection vary with hardware and workload.

## Options and output

```text
./run benchmarks -- performance run [--suite standard|smoke|asset-loading]
    [--workload CASE_ID]
    [--baseline RUN_MANIFEST_OR_DIRECTORY]
    [--source-directory DIRECTORY]
    [--warmup SECONDS]
    [--measure SECONDS]
    [--repetitions COUNT]
```

Timing and repetition options override the `standard` and `smoke` defaults and become part of the compatibility key. `asset-loading` accepts only repetition overrides; its warm-up and measurement are fixed at zero. Each run creates a unique result directory under `$XDG_DATA_HOME/krisp-engine/benchmarks/performance`, falling back to `~/.local/share/krisp-engine/benchmarks/performance` when `XDG_DATA_HOME` is unset. Benchmark assets use the shared `build/benchmark-assets` cache. A selected baseline is read only; the runner never promotes or overwrites one automatically.

Each run directory contains `run_manifest.json`, `report.md`, and a `workers/` tree with separate logs, completion metadata, and raw JSONL records for every repetition. The run manifest is written atomically as workers finish, so a failed or interrupted run retains completed evidence. The baseline can be either a prior `run_manifest.json` path or its run directory.

Preflight failures write a diagnostic manifest and report when output creation succeeded, launch no workers, and return a nonzero status. Worker failures, invalid observations, watchdog timeouts, and interruption also return nonzero while preserving completed results. An advisory regression or slowdown does not affect the exit code. Valid CPU-only results are marked partial when GPU timestamps are unsupported. Missing timestamps on a device that reports support invalidate that repetition's result; they are never replaced with zero or CPU elapsed time.

The coordinator waits up to 300 seconds for Sponza readiness and 120 seconds for other workloads. After readiness, it allows the requested warm-up and measurement durations, up to 30 seconds for final GPU collection, and a 10-second shutdown margin. On interruption or timeout it terminates and reaps the active worker process group.

## Reading results

Raw CPU timestamps and durations use monotonic nanoseconds. Per-repetition output reports successful simulation ticks, graphics submissions and present calls over the measurement window, plus nearest-rank median, p95 and p99 durations for available sample types. Work intervals that touch the measurement boundary are excluded and counted by reason. Graphics active work subtracts the union of its nested waits, API calls, and pacing sleep to avoid double-counting. GPU spans are joined to their original submission and snapshot publication phase, even when results arrive during collection. `UniformUpload` and `CommandRecording` are associated with a frame snapshot, while `ResourceUpload` measures processing during frame acceptance and `SnapshotConstruction` measures building and publishing a snapshot; those latter CPU scopes use only their own time interval for measurement membership.

The `GpuShadow`, `GpuMain`, and `GpuUi` records delimit GPU boundary spans with `BOTTOM_OF_PIPE` timestamps around the corresponding work. They are queue execution spans, not physical scanout measurements. GPU work may overlap or be pipelined across these boundaries, so phase durations are not pure isolated costs and are not guaranteed to add up to the total `Gpu` duration. The total includes other GPU work such as compositing, presentation quads, and capture. Observation remains opt-in and introduces no additional waits. The expanded sample records use protocol 3; baselines from earlier protocols are incompatible.

Deadline output preserves strict positive lateness and separately counts missed deadlines using the default 1 ms tolerance. Skipped schedule slots count as missed opportunities even when a loop iteration did not occur. Repeated and skipped snapshots are counted from successful submissions. Present-call intervals are CPU-side API completion cadence; they are not display or scanout measurements.

Baseline comparisons first compare compatibility fields exactly. Unknown machine, build, workload, protocol, or effective-setting values fail closed and produce field-level reasons. The build manifest fingerprints the Python coordinator, statistics, asset validator, provenance tool, and asset lock; the runner refuses to launch workers when those runtime files differ from the build-time fingerprints. Their hashes participate in baseline compatibility. The environment-lighting file hash and effective renderer settings also participate in compatibility. Compiled SPIR-V hashes are recorded as provenance, while engine source revision, dirty state, and source fingerprint do not need to match between builds. For each available primary metric, the report shows repetition values, median, range, median absolute deviation, absolute and relative difference, and a deterministic 10,000-resample independent-repetition bootstrap 95% interval. At least two repetitions per side are required; a direction is reported only when the interval excludes zero. Labels are advisory and there is no combined score or automatic regression gate.

The report retains debug, validation, presentation-constraint, partial, invalid, and failed states. Standard-suite results describe warmed steady-state runs on the recorded machine; smoke is a short Release lifecycle check. Matching metadata and bootstrap intervals do not remove thermal, OS-load, driver-cache, or other environmental effects.

For unattended Linux correctness checks where the desktop window manager prevents focus, an isolated X11 session can be used:

```sh
xvfb-run -a ./run benchmarks -- performance run --suite smoke
```

Check the recorded device and presentation mode: a virtual display does not establish physical 60 Hz display pacing, and these results must not be interpreted as scanout measurements. Keep the same display setup for both runs being compared. Workers request focus once at readiness, before warm-up; focus loss during measurement remains an invalid result.

### Instantiation attribution

Loading workers additionally record exclusive wall-clock time inside model
instantiation for dependency reads, image decoding, document processing, mesh
conversion, material construction, and ECS adoption. `other_ns` is the remaining
unattributed interval, including scheduling, synchronization and observation
overhead. These seven values sum exactly to `asset_instantiation_ns`.

Nested stage durations exclude child stages; material work inside mesh
conversion, for example, is counted under material construction. Document
processing excludes measured image decoding and dependency reads. Mesh
conversion includes the remaining model conversion and validation work; it is
not a pure vertex-copy measurement. File-read time includes the OS-cache path
and makes no physical disk-throughput claim.

Each successful worker writes `instantiation_trace.jsonl` with monotonic span
boundaries and exclusive `self_ns`, plus `asset_loading.instantiation_breakdown`
(schema version 1, nanoseconds). Reports show the stage durations and shares.
Observation is enabled only for loading-worker instantiation and disabled by
default in other consumers. Schema version 2 and loading instrumentation version
2 prevent comparisons with earlier loading results that lack this observation.

The `image_decode_ns` stage measures the image-loader callback. With prepared
DDS textures it includes copying the compressed payload, rather than PNG
pixel decompression; a nonzero value does not imply source PNGs were decoded.
