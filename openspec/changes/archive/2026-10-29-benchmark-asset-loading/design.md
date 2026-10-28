# Design

## Context

See proposal.md for motivation. The existing `standard` and `smoke` protocols measure rendering after worker readiness and keep asset startup outside their timed distributions. Asset preparation verifies a shared local cache before launch, while the worker builds a scene through `GameEngine`, `AssetRegistry::load_asset`, and `AssetRegistry::instantiate_model`. The engine already tracks readiness for its normal run lifecycle, but consumers cannot read the time at which the initial scene's first presentation was observed.

## Goals / Non-Goals

**Goals:**

- Add an independent loading protocol and result schema while preserving rendering suite behavior and result interpretation.
- Define reproducible CPU wall-clock boundaries around existing application operations and use the engine's first-presentation timestamp for the final readiness boundary.
- Keep app-specific asset selection, scene setup, and benchmark policy in performance mode of the benchmarks application.

**Non-Goals:**

- Measuring physical scanout, isolated GPU upload, GPU query timings, or warmed rendering performance in this suite.
- Controlling or clearing operating-system file caches or claiming cold-cache results.
- Adding asset paths or loading policy to the engine API.

## Decisions

### Use the existing worker and preparation path

Add `asset-loading` to the application runner's suite dispatch and use the same two pinned workload identities as the existing Flight Helmet and Sponza atrium assets. Keep automatic cache preparation and checksum validation in preflight, before any worker starts. Each timed repetition launches a fresh process, but OS cache state is not controlled because preparation and verification read the files; identify this explicitly in run compatibility metadata. This retains pinned inputs and process isolation without overstating cache reproducibility.

### Prepare a versioned DDS runtime texture profile

The performance benchmark asset converter derives all 87 pinned source PNG images as
base-level-only BC3/DXT5 DDS files under profile
`engine-pbr-opaque-bc3-v1`, converter `krisp-benchmark-assets/3`. It preserves
image dimensions, material bindings, and geometry, keeps source PNGs in the
source cache, and emits runtime glTF references using `MSFT_texture_dds` marked
both used and required. There is no PNG fallback: TinyGLTF eagerly decodes
fallback images. BC3 is lossy, including for normal maps, and is an approved
benchmark input choice; no speed improvement is claimed until the prepared
assets are profiled. Exact derived-file hashes and the profile/converter identity
participate in prepared-cache and benchmark compatibility, preventing older
PNG-profile baselines from appearing equivalent.

Pillow 11.3.0 is pinned for PNG-to-DDS conversion and is needed only when new
derived assets must be produced. A valid current cache can be reused without
that package. On a valid cache from the previous known PNG profile, preparation
verifies the cached source files and creates the new cache in a sibling staging
directory. It validates the complete result before publication, retaining the
old cache until then and moving it to a backup at publish time. Missing caches
use normal preparation; corrupt current caches fail closed, and unknown old
profiles or converters are rejected rather than migrated.

### Place CPU timers around current API boundaries

Use monotonic CPU wall-clock timestamps around engine construction, `load_asset`, `instantiate_model`, and remaining scene setup. Define model instantiation to include its actual API work, including materialization, decoding, and ECS adoption; do not infer or subtract substage times that the API does not expose. Define scene setup from completed model instantiation through application scene attachment, lighting, and camera configuration. Compute `asset_to_ready_ns` as registration + instantiation + scene setup + presentation readiness. Compute `engine_to_ready_ns` from engine construction start through the engine-provided presentation timestamp, so its boundary also includes any pre-asset-loading frame.

### Use engine-observed presentation as readiness

Expose an optional monotonic nanosecond timestamp from `GameEngine` after it observes successful presentation containing the initial scene. Store it once and make it queryable to the consumer. The loading worker waits for that timestamp before writing completion metadata. The timestamp marks application observation of a presentation event; it cannot establish physical scanout. Presentation readiness is the elapsed CPU interval from scene setup completion to that observation, including immutable publication, incremental upload, present, and game-thread polling. It is an end-to-end boundary and does not isolate any of those costs.

### Separate loading results from rendering observation

Write a schema-version-1 completion record with seven named nanosecond durations and JSONL timing records for each worker. Loading workers do not enable runtime frame/GPU observation, warm-up, or measurement loops; this avoids adding frame telemetry to the startup being measured. The existing FPS/TPS overlay remains enabled for operator visibility where supported, and its setting is recorded because it is a visible runtime diagnostic. Keep the standard and smoke worker paths, metrics, and existing result schema unchanged.

### Use bounded timeouts and existing comparison summaries

Retain workload startup limits of 120 seconds for Flight Helmet and 300 seconds for Sponza, with the runner's shutdown margin for process termination and reaping. Preserve successful workers when another fails, and never synthesize timing values for a worker that did not reach readiness. Loading baseline identity includes workload, protocol and boundary versions, asset manifest/checksums, machine, driver, build class, and the stated cache policy. Summarize independent repetitions with medians, range, MAD, and the existing bootstrap method.

Alternatives considered: a `cold` versus `warm` cache pair would be nonportable without privileged OS cache control; per-phase GPU timestamps would add instrumentation and still could not cleanly partition incremental upload from frame work. Both would imply precision the current engine and host controls do not provide.

## Risks / Trade-offs

- [OS cache state varies between workers and machines] → Record fresh-process and uncontrolled-cache conditions and require a compatible cache policy for comparisons.
- [First presentation detection may add polling delay] → Timestamp engine observation at the first successful present event and document readiness as application-observed.
- [Long or hung scene loading can delay the suite] → Bound each workload with its existing startup timeout and preserve completed worker results.

## Migration Plan

The valid previous PNG-profile benchmark cache is migrated to the versioned DDS
profile when preparation runs. Its verified source inputs are reused, while
derived files are rebuilt and checksummed in sibling staging before publication;
the prior cache is retained as a backup. Corrupt current caches and caches from
unknown profiles/converters fail closed. Missing caches follow normal pinned
source preparation. The profile, converter version, and exact prepared-file
hashes change asset identity, so earlier PNG-profile loading baselines are
incompatible. Existing standard and smoke metrics and protocols retain their
current semantics; loading records use their versioned schema and compatibility
metadata.

## Instantiation attribution

Loading schema version 2 adds a versioned exclusive instantiation breakdown and
raw spans. A generic registry observer propagates scoped context through CPU
preparation; nested stage scopes subtract child durations. The benchmark
collects only during instantiation, sums exclusive stage times and retains the
unattributed remainder rather than guessing at its cause. Loading instrumentation
version 2 prevents comparisons against the prior observation-free protocol.
