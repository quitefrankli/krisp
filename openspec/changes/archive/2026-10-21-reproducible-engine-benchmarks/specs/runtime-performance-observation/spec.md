# Spec Delta

## Purpose

Expose application-neutral runtime scheduling and measurement mechanisms so consumers can distinguish simulation work, rendering work, waits, and frame cadence without imposing benchmark policy on the engine.

## ADDED Requirements

### Requirement: Explicit opt-in scheduling controls
The engine SHALL expose independent simulation and rendering pacing controls. Consumers SHALL be able to select a 60 Hz fixed simulation timestep with either uncapped rendering or a 60 Hz render target. The engine SHALL report effective settings and preserve its ordinary scheduling when no override is supplied.

#### Scenario: Throughput scheduling
- **WHEN** a consumer requests a 60 Hz simulation target and uncapped rendering
- **THEN** rendering has no engine-imposed frame limit and simulation advances in fixed 1/60-second steps while targeting 60 ticks per wall-clock second
- **AND** failure to achieve that simulation rate is observable rather than concealed by changing the timestep

#### Scenario: Paced scheduling
- **WHEN** a consumer requests 60 Hz simulation and rendering
- **THEN** both targets use a 1/60-second period without rounding to an integer number of milliseconds
- **AND** overruns are observed without an unbounded catch-up burst

#### Scenario: Ordinary consumer
- **WHEN** scheduling overrides and observation are absent
- **THEN** existing runtime scheduling remains in effect and measurement resources are not activated

### Requirement: Independent runtime observation
Observation SHALL preserve independent simulation and graphics execution and latest-wins snapshot consumption. Samples SHALL identify simulation publications and the snapshot used by each graphics submission so consumers can distinguish repeated and skipped snapshots from rendering throughput.

#### Scenario: Graphics consumes snapshots at a different rate
- **WHEN** graphics submits the same snapshot twice or consumes a newer snapshot after missing intermediate publications
- **THEN** emitted submission records retain the associated snapshot identifiers and publication times
- **AND** observation does not introduce a one-tick-to-one-render handshake

### Requirement: Monotonic CPU measurement boundaries
CPU durations SHALL use a monotonic clock. Observations SHALL distinguish simulation work including snapshot publication, graphics active work, frame-fence and image-fence waits, image acquisition, queue-submit and present-call durations, and pacing sleep. Each duration SHALL have a documented boundary and unit.

#### Scenario: Submission is blocked by image acquisition
- **WHEN** image acquisition blocks while rendering is otherwise inexpensive
- **THEN** acquisition wait is separately observable from CPU work and GPU execution
- **AND** enclosing elapsed durations are identified so they are not incorrectly summed with their child intervals

### Requirement: Asynchronous GPU execution measurements
The engine SHALL report graphics-command execution durations using GPU timestamps when supported. Results SHALL be associated with their original submission, interpreted using device timing properties, and retrieved after ordinary submission completion without adding per-frame completion waits. Unsupported or missing results SHALL be explicit rather than zero durations.

#### Scenario: Delayed result collection
- **WHEN** a measured submission completes after newer submissions have been issued
- **THEN** its GPU duration is attached to its original submission and measurement phase
- **AND** query resources are not reused before their results are safely collected

#### Scenario: GPU timestamps are unsupported
- **WHEN** the selected graphics queue lacks usable timestamp support
- **THEN** observation reports GPU measurement as unavailable with a reason while retaining valid CPU records

#### Scenario: Timestamp counter wraps
- **WHEN** timestamps wrap within the supported counter width
- **THEN** duration conversion respects that width and the device timestamp period

### Requirement: Honest frame cadence events
Observations SHALL distinguish successful submissions and successful present-call completion events from unsuccessful attempts. CPU present-call cadence SHALL NOT be described as measured physical display or scanout cadence. Each event SHALL carry a monotonic timestamp.

#### Scenario: A frame cannot be presented
- **WHEN** acquisition, submission, or presentation fails
- **THEN** the failed attempt is not counted as a successful presentation and the failure is available to the consumer

### Requirement: Bounded optional observation
Observation SHALL use bounded buffering with observable overflow or lost-sample counts. Export and summary generation SHALL be kept outside measured frame work. Starting and stopping collection SHALL be explicit, and stopping SHALL account for outstanding measured submissions without including teardown in their durations.

#### Scenario: Consumer cannot keep up
- **WHEN** observation buffering exhausts capacity
- **THEN** dropped records are counted and surfaced rather than silently discarded or causing unbounded memory growth

#### Scenario: Last measured frames are still in flight
- **WHEN** collection ends before their GPU results become available
- **THEN** collection resolves or explicitly marks those results missing and retains their original measurement membership

### Requirement: Effective runtime metadata
The engine SHALL expose the effective graphics extent, graphics settings, presentation mode, selected device and driver identity, timestamp capabilities, validation state, and scheduling settings needed to interpret measurements.

#### Scenario: Requested and effective settings differ
- **WHEN** the platform cannot supply a requested presentation mode or extent
- **THEN** metadata reports both the requested and effective settings and the reason for the difference
