# Runtime options and observation

`RuntimeOptions` provides optional, application-neutral controls for runtime
scheduling and window setup. An unset rate leaves the existing scheduling path
in effect. A render rate of zero means uncapped rendering. A fixed simulation
timestep requires a positive simulation rate. `validate_runtime_options()`
checks rates and positive window dimensions that fit the platform window API.
Optional vertex and index buffer capacities are byte counts. Supplied values
must be positive, four-byte aligned, and representable by the host allocation
size. Effective capacities belong in `RuntimeGraphicsInfo` so a capture records
the resources actually configured by its consumer.

`RuntimeOptions.performance_overlay` opts in to the engine FPS/TPS display and
defaults to `false`. Benchmark workers enable it explicitly; their displayed
rates are diagnostic and do not replace the raw observation records. Overlay
updates and drawing occur inside normal frame work and their CPU/GPU cost stays
in observations. A benchmark consumer should record the effective overlay
setting with the rest of its runtime configuration.

`RuntimePacer` schedules against absolute `steady_clock` deadlines. It retains
fractional nanoseconds between periods, so a 60 Hz schedule reaches each second
boundary without accumulating the rounding error of a rounded 16 ms or
16,666,667 ns period. `advance(now_ns)` is the deterministic scheduling path for
callers that already have a monotonic timestamp. It returns the deadline to
target and counts slots skipped when the caller is late. `wait()` applies the
same arithmetic and sleeps until the returned absolute deadline.

## Collection

Runtime observation is optional. Constructing a `RuntimeObservation` allocates
two fixed-capacity buffers, one for the game thread and one for the graphics
thread. No observer means no measurement buffers. Call `start()` and `stop()`
explicitly around a collection. Recording while stopped is ignored; stopping
does not discard buffered records, so the consumer can drain them afterward.

Each producer is single-threaded and writes only to its own ring. Recording
copies a fixed-size `RuntimeSample` without allocating or waiting for the
consumer. When a ring is full, that sample is dropped and `lost_records()` is
incremented. The consumer calls `drain()` outside measured frame work; it gets
the currently available records sorted by their monotonic start timestamps.
The buffers are bounded independently, so one producer cannot consume the
other producer's capacity.

CPU timestamps and durations use monotonic nanoseconds. Unless a sample is a
GPU record, its elapsed duration is `end_ns - begin_ns`. GPU records carry a
separately converted `duration_ns`; GPU timestamps are not CPU timestamps and
must not be compared directly with `begin_ns` or `end_ns`.

The sample kinds identify these measurement boundaries:

| Kind | Boundary |
| --- | --- |
| `Simulation` | Simulation update through publication of its render snapshot. |
| `Graphics` | Graphics-thread active interval selected by the caller; it may enclose child waits and calls. |
| `FrameFenceWait` | CPU time waiting for the frame's reusable submission fence. |
| `ImageFenceWait` | CPU time waiting for the fence associated with an acquired image. |
| `Acquire` | Image acquisition call, including any time blocked inside the call. |
| `Submit` | Queue submission call duration. |
| `Present` | Presentation call duration and its success status; this is not physical display or scanout cadence. |
| `GameSleep`, `GraphicsSleep` | Time spent in the corresponding pacing sleep. |
| `GameDeadline`, `GraphicsDeadline` | Pacing deadline and skipped-slot count. |
| `Gpu` | Asynchronously collected GPU execution duration for `submission_id`. |

Intervals can be nested. For example, `Graphics` may contain `Acquire`,
`Submit`, and `Present`; summing those records with the enclosing interval would
double-count time. Failed attempts retain `success = false` and are not
successful presentations. Snapshot association is carried by `frame_number`
and `publication_ns`; simulation records can also carry `tick_index`.

`timestamp_duration_ns()` converts a GPU counter delta using the queue's valid
counter width and the device timestamp period. It supports wrapping counters,
including a 64-bit counter, and returns no value when timing properties are
unavailable or invalid. The caller remains responsible for retrieving query
results only after the associated work completes and for retaining each result's
original submission and collection membership. With a counter narrower than 64
bits, the utility assumes the interval spans fewer than one full counter wrap;
the raw timestamps alone cannot reveal multiple wraps. Frame collection also
checks that the enclosing CPU interval is shorter than one wrap period before
accepting a GPU duration.

Graphics frames use two timestamp queries around command execution when the
observer is configured and the graphics queue reports timestamp support. The
frame reads results only after its existing submission fence signals, using
availability queries without a wait flag. It retains the query pool until the
result is collected or explicitly marked unavailable, then reuses the pool for
that frame's next submission. This adds no per-frame fence or device-idle wait.
For GPU samples, `begin_ns` is the CPU timestamp taken at the start of the queue
submit call, while `end_ns` is when the completed query is collected; only
`duration_ns` represents GPU execution time. The sample keeps the submission
attempt and render snapshot identifiers from the original submission.

## Initial-scene presentation readiness

`GameEngine::get_ready_timestamp_ns()` returns zero until the engine has
observed a successful presentation containing its initial scene. It then
returns the monotonic nanosecond timestamp recorded for that first observation;
later presentations do not replace it. This timestamp uses the engine's
monotonic clock domain and remains available to the consumer after readiness.
It marks application observation of presentation, not physical display scanout
or an application-specific readiness policy.

## Resource loading stage observation

`AssetRegistry::set_load_observer` enables optional resource loading spans for
that registry. Callbacks may execute on the application thread or its loading
worker and must be thread-safe. Each sample identifies a generic stage and
monotonic begin/end timestamps; `self_ns` excludes nested observed stages and
callback overhead. Scoped observer context propagates through CPU preparation
and is restored when work ends. Consumers choose collection and reporting
policy; the engine contains no benchmark asset or report policy.
