# GPU modernization benchmark comparison

Date: 2026-11-05. This compares the aggregate effect of dynamic rendering, VMA, and Synchronization2; it does not isolate the contribution of each change.

- Before: `2cf29df146022b2f9fce1f1021c7ddc1f67795d4`.
- After: `771cbdfe7279b5aacafa1dbe4b495e4d711075da`.
- Both source trees were clean when built. All 36 workers completed successfully: 24 standard workers and 12 loading workers.

## Findings

No throughput FPS comparison has a bootstrap interval excluding zero. The synthetic scenes used less GPU time, while CPU graphics active work increased. Flight Helmet reached its first scene presentation sooner; Sponza loading did not show a clear change.

Uniform-upload CPU time increased roughly fivefold in all three throughput workloads. This is a useful profiling target; these aggregate measurements do not establish which change caused it.

## Protocol and limits

- CPU: AMD Ryzen 5 3600; GPU: NVIDIA GeForce RTX 5070 Ti; NVIDIA driver 595.99.02.
- Compiler: Clang 17.0.6; Meson Release, optimization 3, assertions and Vulkan validation disabled. Conan dependencies and native file were shared.
- Standard: 1280×720, 4× MSAA, 1024 shadow maps, performance overlay enabled, UI disabled. Three fresh processes per workload/mode, each with 2 seconds of warm-up and 10 seconds of measurement.
- Loading: three fresh processes per asset, ending at initial scene presentation; no steady-state measurement or GPU telemetry.
- One private Xvfb display and one shared verified asset cache. Both sides report matching effective settings within each workload/mode. Loading uses the same FIFO fallback on both revisions.
- Order: before standard, after standard, after loading, before loading. Suite-start GPU temperatures were 39, 42, 43, and 44 °C respectively. Clocks and thermal conditions were not locked.
- OS file-cache state is uncontrolled. These are not cold-cache results, physical scanout measurements, or memory-usage measurements.
- Values below are medians of independent process-level measurements. Intervals are 95% independent-repetition bootstrap intervals for after-minus-before using 10,000 resamples. Three repetitions give limited evidence, and many metrics are inspected.

The built-in automatic baseline report rejects the intentional VMA dependency addition and its derived build configuration fingerprint. The separate analysis preserves the original manifests, verifies that the sole dependency delta is `vulkan-memory-allocator/3.3.0`, and checks every other compatibility field. No other mismatches or unknown compatibility values were found. The results below are an explicitly documented aggregate comparison including that dependency change, not an automatic baseline pass.

## Frame submissions per second

These rates count successful graphics submissions; they are not physical display FPS.

| Workload | Mode | Before | After | Change | Difference 95% interval |
| --- | --- | ---: | ---: | ---: | ---: |
| Moving transforms, 500 | throughput | 38.200 | 38.300 | +0.26% | [-0.100, +0.200] |
| Point shadows, 500 | throughput | 38.100 | 37.900 | -0.52% | [-0.400, +0.800] |
| Shadowed Sponza | paced | 30.000 | 30.000 | +0.00% | [-0.100, +0.100] |
| Shadowed Sponza | throughput | 35.500 | 35.400 | -0.28% | [-0.500, +0.600] |

## GPU and CPU work

| Workload | Metric (ms) | Before | After | Change | Difference 95% interval (ms) |
| --- | --- | ---: | ---: | ---: | ---: |
| Moving transforms, 500 | GPU span | 0.143 | 0.127 | -11.26% | [-0.019, -0.005] |
| Moving transforms, 500 | CPU graphics active | 1.663 | 1.757 | +5.66% | [+0.045, +0.237] |
| Point shadows, 500 | GPU span | 0.213 | 0.185 | -13.19% | [-0.130, -0.011] |
| Point shadows, 500 | CPU graphics active | 1.721 | 1.938 | +12.62% | [+0.212, +0.427] |
| Shadowed Sponza | GPU span | 2.696 | 2.707 | +0.42% | [+0.008, +0.012] |
| Shadowed Sponza | CPU graphics active | 1.907 | 2.023 | +6.08% | [-0.053, +0.228] |

The synthetic GPU reductions are only about 0.016–0.028 ms per frame. Sponza GPU time increased by about 0.012 ms in throughput mode; this small effect should not be treated as a robust hardware-independent regression.

| Throughput workload | Uniform upload before (ms) | After (ms) | Ratio | Present call before (ms) | After (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Moving transforms, 500 | 0.0400 | 0.2026 | 5.06× | 24.458 | 24.309 |
| Point shadows, 500 | 0.0353 | 0.1984 | 5.62× | 24.489 | 24.190 |
| Shadowed Sponza | 0.0311 | 0.1571 | 5.05× | 26.008 | 26.087 |

Present calls consume roughly 24–26 ms, far longer than the measured GPU work. This indicates that presentation dominates the observed frame period in this virtual-display setup, so the unchanged submission rates do not establish unchanged maximum rendering throughput on a regular display. The CPU and GPU work measurements are more informative about the internal changes.

## Asset loading

| Asset | Metric (ms) | Before | After | Change | Difference 95% interval (ms) |
| --- | --- | ---: | ---: | ---: | ---: |
| Sponza | Engine to ready | 5658.024 | 5646.791 | -0.20% | [-84.689, +11.246] |
| Sponza | Asset to ready | 5230.474 | 5197.278 | -0.63% | [-78.062, +21.276] |
| Sponza | Presentation readiness | 2646.714 | 2646.472 | -0.01% | [-26.664, +5.012] |
| Sponza | Engine initialization | 375.638 | 371.715 | -1.04% | [-21.615, +28.868] |
| Flight Helmet | Engine to ready | 744.991 | 696.469 | -6.51% | [-56.852, -14.924] |
| Flight Helmet | Asset to ready | 294.908 | 215.294 | -27.00% | [-80.563, -50.325] |
| Flight Helmet | Presentation readiness | 193.420 | 113.211 | -41.47% | [-81.354, -51.711] |
| Flight Helmet | Engine initialization | 390.741 | 420.512 | +7.62% | [+22.904, +38.251] |

Flight Helmet's presentation-readiness phase fell from 193.4 to 113.2 ms, while its engine initialization increased from 390.7 to 420.5 ms. The resulting end-to-end improvement was 6.5%. Presentation readiness includes publication, incremental GPU uploads, presentation, and game-thread observation; it is not an isolated GPU upload measurement. Sponza's end-to-end loading interval includes zero change.

## Reproduction and evidence

Run the same commands from each source checkout after configuring its `build/release` with the shared native file and linking its `build/benchmark-assets` to the shared cache:

```sh
./run benchmarks -- performance run --suite standard --repetitions 3
./run benchmarks -- performance run --suite asset-loading --repetitions 3
```

The local orchestrator ran both checkouts in one `xvfb-run -a` session. Evidence remains under `build/benchmark-comparison/`:

- `runs.json`: paths to all four original manifests, reports, raw JSONL records, and worker logs.
- `comparison.json`: original compatibility differences, raw repetition values, and bootstrap comparisons.
- `comparison.md`: full generated tables and provenance.
- `analyze.py`: analysis without modifying original manifests.
- `run_comparison.py`: source checkout, order, commands, and display/data settings.
