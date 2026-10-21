# Proposal

## Why

The current benchmark suite measures warmed rendering behavior but leaves asset loading as an unmeasured startup cost. A repeatable, fresh-process suite is needed to compare representative asset loads and distinguish CPU setup stages from the time until the scene is first observed as presented.

## What Changes

- Add a Release-only `asset-loading` suite that launches one fresh worker per repetition for the Flight Helmet and Sponza atrium workloads, with two repetitions by default.
- Record monotonic nanosecond timings for engine construction, asset registration, model instantiation, scene setup, presentation readiness, and total time to readiness in each worker's completion record.
- Define readiness as the application observing the first presentation containing the initial scene; do not describe it as physical scanout or isolated GPU upload time.
- Keep `--workload` and `--repetitions` available, and reject warm-up or measurement overrides for this suite. Leave standard and smoke rendering protocols unchanged.
- Exclude asset preparation and checksum verification from timed regions, record that OS cache state is uncontrolled, and use the existing compatible-baseline and repetition-summary mechanisms.
- Expose an application-neutral engine timestamp for when first initial-scene presentation is observed.
- Prepare the benchmark's 87 source PNG textures as base-level-only BC3/DXT5 DDS runtime images under profile `engine-pbr-opaque-bc3-v1`, with the profile and converter version included in prepared-asset identity.
- Migrate a valid cache from the previous PNG profile from its verified cached sources using sibling staging and backup-preserving publication; reject corrupt current caches and unknown prior profiles.
- Keep original PNGs as source inputs, while the derived runtime glTF requires `MSFT_texture_dds` and contains no PNG fallback; acknowledge that BC3 is lossy, including for normal maps.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `performance-benchmarking`: add an asset-loading suite and its timing, result, compatibility, and failure behavior.
- `runtime-performance-observation`: expose the monotonic timestamp when the initial scene's first presentation is observed.

## Impact

The stress-test application gains a new suite, worker lifecycle metadata, loading result schema, compatibility identity, timeout handling, DDS benchmark-asset preparation and documentation. The engine adds a generic readiness timestamp accessor. Existing rendering metrics and their standard/smoke protocols remain unchanged. No asset-loading speed improvement is claimed before profiling the prepared assets.
