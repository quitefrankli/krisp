# Tasks

## 1. Window state and camera

- [x] 1.1 Publish coherent framebuffer/iconify state and generation from GLFW callbacks, keeping logical dimensions separate; verify coalescing, zero-size handling and shutdown wake-up with focused state tests and native callback inspection.
- [x] 1.2 Add validated camera aspect updates preserving perspective FOV and orthographic vertical span; update on the game thread before snapshot publication and guard cursor normalisation; verify both projections and picking with focused camera tests.
- [x] 1.3 Document thread ownership and logical/framebuffer size semantics in docs/GRAPHICS_ENGINE.md; verify documentation matches the implemented interfaces.

## 2. Presentation resource ownership

- [x] 2.1 Replace static extent caching and placement-new reset with explicit recreation and surface-limit selection, preserving valid constructor ordering; verify chosen extents and image counts with focused selection tests.
- [x] 2.2 Add renderer release/rebuild hooks for window-sized resources and descriptors, preserving assets and settings; verify resize-return cycles refresh presentation/quad/GUI bindings without retained obsolete attachments.
- [x] 2.3 Reconcile actual image-count changes across frames, global/renderable/skeleton buffers, shadow/particle resources and ImGui; verify differing image counts through a focused lifecycle integration fixture.
- [x] 2.4 Require swapchain maintenance support and implement present-resource retirement using presentation completion fences and safe partial-failure cleanup; verify repeated recreation with validation enabled, including consecutive resizes before a successful presentation.
- [x] 2.5 Document resource ownership and recreation dependencies in docs/GRAPHICS_ENGINE.md; verify no per-frame idle waits or scene-asset rebuilds were introduced.

## 3. Rendering recovery and suspension

- [x] 3.1 Make viewport/scissor dynamic for engine pipelines and record each pass's own extent, including shadow, particle, quad and presentation paths; verify unchanged-size rendering and a non-square resize retain orientation and shadow dimensions.
- [x] 3.2 Handle acquire/present OUT_OF_DATE and SUBOPTIMAL with explicit draw outcomes, preserving semaphore consumption and fence reset ordering; verify skipped attempts do not advance readiness or successful-presentation counters.
- [x] 3.3 Suspend rendering without busy-spinning on iconify/zero-size, restore using latest state, and wake on shutdown while game events/simulation/audio continue; verify minimise/restore, startup minimisation and close while suspended through integration checks.
- [x] 3.4 Update graphics documentation for suspension and recovery behaviour, and PERFORMANCE.md for transient recreation costs and steady-state mechanisms; verify claims distinguish measured results from expectations.

## 4. Capture and UI lifecycle

- [x] 4.1 Finalise active video recording on framebuffer dimension change and release session waiters without terminating rendering; verify completed output retains original dimensions and UI recording state clears.
- [x] 4.2 Drain submitted captures with original extents, explicitly fail unsubmitted blocking capture requests during suspension/recreation, and release all waiters at shutdown; verify resize/minimise during recording, screenshot and frame-capture integration cases.
- [x] 4.3 Document recording/capture transitions in graphics documentation; verify previews use replacement image bindings after resize.

## 5. Integration and performance evidence

- [x] 5.1 Build with `meson compile -C build/debug -j 6 sandbox` and run `meson test -C build/debug -j 6`; verify all required checks pass without repeating identical successful checks.
- [x] 5.2 Exercise repeated resize/maximise/restore, minimised close and loading, recording transitions, differing aspect ratios and available display scaling with Vulkan validation enabled; record coverage and any unavailable platform cases, checking stable resource usage after retired generations drain.
- [x] 5.3 Repeat standard and asset-loading Release protocols with the latest explicit baseline paths from the design; verify compatibility and worker validity, and record throughput, CPU/GPU work, tails and pacing results in PERFORMANCE.md without claiming a proven 3% gate.
- [x] 5.4 Measure resize stalls and minimised CPU/GPU activity separately, document observed limitations, and inspect the final diff for application-neutral behaviour, preserved unrelated edits and disabled ray tracing.

## Verification evidence

- Debug sandbox and graphics integration executable built; all 10 Meson suites passed.
- Native validation runs covered resize/maximise/restore, startup minimisation, minimised shutdown, a controlled zero-to-three image-count transition, recording, PNG screenshots and production frame capture. Final logs contain no Vulkan warnings/errors. Fractional DPI and different nonzero image counts were unavailable and are documented.
- Settled minimisation recorded zero submissions and continuing simulation; resize-return allocation counts remained 33/33. Event recreation logs and CPU interval observations are documented in PERFORMANCE.md.
- Final standard and loading comparisons plus focused repeats and a fresh build of the original revision are retained with explicit compatibility checks. Loading increases against the saved baseline also occurred in the original revision under current host conditions; no precise 3% guarantee is claimed.
- Existing performance baseline documentation was preserved. No commits or ray-tracing enablement were performed.
