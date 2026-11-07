# Design

## Context

See proposal.md for motivation. Window callbacks are disabled; swapchain extent is static, recreation is a stub, pipelines bake viewport/scissor state, and camera aspect ratio is immutable. Frames append renderer resources, and buffers/descriptors are indexed by actual swapchain image count. GLFW event polling runs on the game thread; Vulkan rendering runs on a separate thread.

## Goals / Non-Goals

**Goals:** Explicit ownership of presentation resources, event coalescing, safe restoration and negligible unchanged-window overhead.

**Non-Goals:** Fixed internal resolution/upscaling, fullscreen mode management, resolution menus, application pause policy, new ray tracing paths, compatibility shims or asynchronous recreation for seamless dragging.

## Decisions

### Publish window state; own Vulkan recovery on the graphics thread

Add a coherent framebuffer-size/iconified snapshot with a generation counter, published from GLFW callbacks on the event thread. Query initial framebuffer dimensions on that thread; callbacks perform no GPU work. Keep logical window dimensions for cursor normalisation. Consume only the latest snapshot at a graphics frame boundary and acknowledge only the generation actually rebuilt, so events arriving during recreation are retained. Do not hold the ImGui state mutex while waiting for restoration or GPU completion.

Suspension uses a condition variable notified by state changes and shutdown, rather than uncapped polling. Continue accepting latest scene publications and servicing completed-resource/capture bookkeeping; no new acquisition, drawing or presentation occurs. Existing game-thread polling and simulation/audio continue. Startup may await its first presentation while minimised but must still service close requests. Reset render pacing deadlines on resume to avoid catch-up bursts.

### Explicit swapchain and per-image resource lifetime

Replace the permanent static extent and placement-new reset with owned active extent and explicit recreation. Use framebuffer dimensions when surface extent is unspecified, clamped to surface limits; defer creation for zero dimensions. Re-query surface capabilities and clamp desired image count to supported limits. Keep startup constructor ordering valid: extent must be available before extent-dependent allocations, without reading an unconstructed swapchain member.

Coalesce events and rebuild at most once per frame boundary. On acquisition OUT_OF_DATE, return without submission or resetting a fence. On SUBOPTIMAL acquisition, consume the acquired semaphore through normal submission before scheduling recovery. Handle present OUT_OF_DATE/SUBOPTIMAL as recovery; return an explicit draw outcome so readiness, capture and observation code record actual progress only.

Wait for outstanding GPU work during recreation; accept a transient resize stall rather than introduce concurrent old/new attachment generations. Rendering completion alone does not prove presentation completion. Require KHR or EXT swapchain_maintenance1 support and enable its presentation fences. Wait for pending presentation fences before replacing or destroying frames, including shutdown while minimised. Reject unsupported devices explicitly. This user-approved requirement replaces successor-reacquisition retirement chains and avoids their shutdown limitation.

Add explicit renderer resource release/rebuild hooks. Rebuild window-sized colour/depth/MSAA/resolve/quad attachments, swapchain views, presentation descriptors and GUI image bindings in dependency order. Reconcile global, renderable and skeleton per-image buffers/descriptors, shadow and particle resources, and ImGui image-count state when the actual count changes. Update the bundled Vulkan backend render-buffer count without shutting it down: backend shutdown destroys platform windows and invalidates GLFW handles. Preserve meshes, textures, scene definitions, pipeline caches and renderer selection/settings. With unchanged image count retain window-independent resources such as shadow attachments. Drain completed query and capture results before destroying their owners. Use RAII cleanup for partial recreation failures and report unrecoverable errors through existing shutdown paths.

### Dynamic viewport and scissor

Enable dynamic viewport/scissor for engine graphics pipelines and set them once per rendering pass using that pass's extent, preserving the negative-height convention. Shadow passes use shadow resolution, not window extent. Include particle, quad and presentation paths; ImGui retains backend-owned viewport/scissor handling. Size changes do not rebuild otherwise compatible pipelines. Format/sample configuration changes still require compatible pipelines.

### Camera, UI and captures

Add Camera::set_aspect_ratio with positive finite input validation; update both projections while preserving vertical field of view and orthographic vertical span/zoom. Apply framebuffer changes on the game thread before publishing the next immutable RenderFrame. Do not mutate camera state from the graphics thread. A transient previously published projection may be consumed until the next publication; steady-state projection/picking must agree with the current framebuffer. Guard zero logical dimensions in cursor normalisation.

Refresh descriptor-backed GUI previews after attachment replacement. Finalise submitted video frames before replacing resources and stop/finalise the recorder before drawing a different actual framebuffer extent; clear recording session/UI state and release blocked recording waiters without throwing a fatal resolution-change exception. Minimising alone suspends recording output without synthesising frames. Drain submitted screenshots/frame captures using their original extents. Fail unsubmitted synchronous capture requests explicitly on suspension/recreation so they cannot block event processing; shutdown fails all remaining waiters.

### Performance evidence

Use the latest baseline directories documented in docs/PERFORMANCE.md: standard run-20261106T155810Z-5b5ee753 and loading run-20261106T161914Z-4dacdf07, with control run-20261106T160847Z-e64c88ae. Preserve protocol, workload and runtime tool fingerprints. Repeat the same Release commands and isolated X11 conditions after implementation, checking compatibility and per-worker validity. The user accepts existing variance for now; 3% remains an investigation threshold, not a proven regression gate. Report resize stalls and minimised CPU/GPU activity separately from unchanged-window comparisons.

## Risks / Trade-offs

- Resize stalls → Keep recreation infrequent, coalesce events, retain compatible pipelines and assets; defer asynchronous recreation until measurements justify it.
- Presentation retirement and image-count changes → Explicit retirement ownership plus repeated-resize validation with Vulkan validation enabled.
- Loading/capture deadlocks → Wake shutdown and capture waiters independently of presentation; exercise minimise during startup and recording.
- Noisy baselines → Preserve raw evidence and confidence intervals; do not infer equivalence from an inconclusive comparison.

## Migration Plan

No persisted format or consumer migration is required. Replace internal fixed-size assumptions directly. Build and test in Debug, validate native window transitions, then compare Release performance before completing the change. Preserve unrelated performance documentation edits and do not commit without user consultation.
