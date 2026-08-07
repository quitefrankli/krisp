# Render Boundary

The game thread is the sole producer of render state. At the end of each
completed game update it publishes an immutable `RenderFrame` through a
latest-wins mailbox. Publication never waits for graphics, and loading the
latest publication never waits for the game thread.

Each frame contains all state needed by rasterization: camera and light state,
particle instances, one state and immutable definition per
`RenderableID`, and animated bone poses keyed by `SkeletonID`. A renderable
definition retains its mesh and material handles and records optional
`ObjectID` grouping metadata and an optional skeleton binding. Renderable state
contains its already-composed world transform and effective visibility.
The frame also carries complete presentation state: the desired raster render
mode and the full set of object IDs that receive stencil highlighting.

`RenderableID` is an immutable topology identity across the boundary. Mesh,
materials, pipeline, grouping metadata, and skeleton binding cannot change for
that ID; replacement creates a new ID. `SkeletonID` likewise identifies one
immutable bone hierarchy and inverse-bind layout, while its pose remains
mutable. The mailbox rejects changed definitions and retired-ID
reintroduction. `ObjectID`
is not a graphics ownership key: graphics uses it only to group renderables for
object-level controls such as stencil selection and active-light
shadow suppression. Effective object visibility is composed before publication.
Standalone renderables have no group and are unaffected by object-level state.

The graphics thread loads at most one completed publication at the start of a
graphics-loop iteration. It retains that same `shared_ptr<const RenderFrame>`
while recording every pass, so a draw cannot mix camera, transform, visibility,
particle, or bone data from different game updates. If several game frames are
published during a slow draw, only the newest is accepted by the next graphics
iteration.

Normal publication is latest-wins and never blocks. Deterministic screen
recording adds a separate acknowledgement rendezvous without adding another
state queue: the game thread publishes one fixed-step snapshot and waits for
graphics to copy that exact `frame_number` before advancing again. Stopping or
shutting down cancels the wait. The mailbox and its immutable frame contract
are otherwise unchanged.

Graphics-owned renderables are reconciled independently when renderable
membership changes. Replacing or removing one attachment
does not recreate other renderables in its object group. Dynamic-only frames
reuse the existing transform buffers and descriptor sets.

Skeleton GPU resources are independently keyed by `SkeletonID`. One bone-buffer
slot per skeleton and swap-chain frame is updated from the published pose and
bound by every renderable that references that skeleton. Skeleton membership
changes reconcile the affected shared skeleton resource without making a
skeleton the owner of a renderable.

Removed renderable, skeleton, mesh, and material GPU allocations are retired by
the graphics thread after the last potentially referencing submission fence
completes; topology reconciliation does not wait for the entire device.

There is no second game-to-graphics state queue. Presentation state is part of
the immutable frame, while shutdown uses a dedicated atomic stop request.
Graphics code must not read mutable game objects, the ECS, or the game camera.

Ray tracing is currently unsupported and excluded from C++, shader, pipeline,
renderer, descriptor, and device-feature setup. The supported render boundary
is rasterization.
