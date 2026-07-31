# Architecture TODOs

## Encapsulate mutable skeletal poses

Animation updates and render-frame pose snapshots are game-thread confined,
and graphics consumes only copied bone transforms. However, `get_bones()` still
returns a mutable vector reference, so pose mutation is not expressed through
an explicit skeletal-component operation.

Replace direct mutable access with explicit pose operations. This should
include animation updates, IK, editor manipulation, deserialization, and
tests. Once all callers use those operations, keep bone storage private without
a mutable-reference escape hatch. Add synchronization only if pose mutation is
later moved off the game thread.

## PBR

## Deprecate Graphics Command Queue + Commands

## mutexes and lockguards in GUI and common.hpp

## Remove static Material/Mesh Systems

## Add serialization support for procedurally generated resources
### Add Cache for procedurally generated resourcees

## Deferred graphics resource retirement

Remove runtime `vkDeviceWaitIdle()` calls from topology reconciliation and
unused-resource cleanup. Topology changes may be frequent, and waiting for the
entire device stalls every frame in flight in the swapchain even when only an old resource
generation needs to be released.

Implement fence-driven deferred destruction:

<!-- - Assign a monotonically increasing submission serial to each graphics queue
  submission and associate it with the corresponding frame fence. -->
<!-- - After waiting for a frame fence, advance the completed submission serial.
  Graphics queue ordering means completion of serial N also completes all
  earlier submissions on that queue. -->
- When a renderable, definition, skeleton, mesh, material, descriptor, buffer,
  or texture
  becomes unused, stop referencing it immediately but move its graphics-owned
  resources into a retirement batch tagged with the latest submitted serial.
- Destroy retirement batches only after their serial is completed. Asset IDs
  are not reused, so their backing allocations can use the same mechanism.
- Allow old and new definition generations to coexist while work using the old
  generation remains in flight. Allocation keys must include the definition
  version/generation (or use opaque allocation handles) rather than relying
  only on renderable/frame indices.
- Rebuild command buffers and topology against the new generation without
  destroying resources referenced by earlier submissions.
- Keep `vkDeviceWaitIdle()` only for shutdown and device/swapchain operations
  that genuinely require a device-wide synchronization point, then flush all
  remaining retirement batches.

Add focused tests for the pure retirement bookkeeping: resources are retained
before their completion serial, released once it completes, ordered batches
are drained correctly, and rapid successive definition generations can
coexist safely.

Acceptance:

- Normal topology and unused-resource cleanup never call
  `vkDeviceWaitIdle()`.
- In-flight frames cannot observe freed resources, and slow rendering does not
  block game updates.
- Retired resources are eventually reclaimed after fence completion.
- Multiple rapid membership or definition changes remain valid.
- The application and tests pass without Vulkan validation errors.
