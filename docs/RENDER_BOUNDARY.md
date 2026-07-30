# Render Boundary

The game thread is the sole producer of render state. At the end of each
completed game update it publishes an immutable `RenderFrame` through a
latest-wins mailbox. Publication never waits for graphics, and loading the
latest publication never waits for the game thread.

Each frame contains all state needed by rasterization: camera and light state,
object visibility and local hierarchy transforms, particle instances, animated
bone poses, and shared immutable object/skeleton definitions. Definitions own
their mesh and material handles and carry monotonically changing versions.

The graphics thread loads at most one completed publication at the start of a
graphics-loop iteration. It retains that same `shared_ptr<const RenderFrame>`
while recording every pass, so a draw cannot mix camera, transform, visibility,
particle, or bone data from different game updates. If several game frames are
published during a slow draw, only the newest is accepted by the next graphics
iteration.

Graphics-owned objects are reconciled only when object membership, object
definition versions, skeleton membership, or skeleton definition versions
change. Dynamic-only frames reuse existing buffers and descriptor sets.
Topology replacement waits for graphics-device synchronization before
replacing affected graphics resources. Mesh and material GPU allocations are
retired by the graphics thread after their final immutable CPU owner is
released and the device is synchronized.

The graphics command queue is not a scene-state channel. It remains only for
control operations such as shutdown, render-mode changes, previews, and
ID-only stencil selection. Graphics code must not read mutable game objects,
the ECS, or the game camera.

Ray tracing is currently unsupported and excluded from C++, shader, pipeline,
renderer, descriptor, and device-feature setup. The supported render boundary
is rasterization.
