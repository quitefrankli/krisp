# Architecture TODOs

## Separate imported scene nodes from gameplay objects

The resource loader currently blurs several distinct concepts:

- A glTF mesh is reusable vertex and index data.
- A glTF node is a transformed scene-graph instance that may reference a mesh
  and skin.
- A Krisp `Renderable` describes one draw, usually corresponding to one glTF
  mesh primitive, but has no local transform.
- `ResourceLoader::LoadedMesh` is misleadingly named: it represents a glTF
  mesh-node instance and contains the node transform plus its renderables.
- A Krisp `Object` supplies the only transform shared by a group of
  renderables.

Consequently, applications currently apply `LoadedMesh::transform` to an
`Object`. This is unsafe when that object also represents gameplay state:
colliders, movement, cameras, and other systems inherit an import-specific
transform. For example, `npc.glb` rotates its skinned mesh node by -90 degrees
around X; applying that transform to `PlayerCharacter` also rotates its
Y-aligned capsule.

Krisp currently works around this by keeping `PlayerCharacter` at the gameplay
origin and placing the imported renderables and node transform on a child
visual `Object`.

Refactor the rendering/import model so an imported node instance has an
explicit local transform independent of the gameplay object's world transform.
Possible approaches include:

- Introduce a render-instance or scene-node type containing a local transform
  and one or more `Renderable`s.
- Add transformed renderable groups to `Object`.
- Extend `Renderable` with a local transform, accepting that the node transform
  would be repeated for every primitive.

The renderer should ultimately compose:

```text
gameplay world transform * imported node local transform
```

Keep mesh assets immutable and reusable; do not bake node transforms into
shared vertex data. The refactor must also preserve skinning transforms,
normal transformation, bounds, picking, serialization, and multi-node
instances of the same mesh.

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

## No Object in Graphics

graphics thread shouldn't know about gameplay objects. It should only know about renderables and their associated resources

## Deferred graphics resource retirement

Remove runtime `vkDeviceWaitIdle()` calls from topology reconciliation and
unused-resource cleanup. Topology changes may be frequent, and waiting for the
entire device stalls every frame in flight even when only an old resource
generation needs to be released.

Implement fence-driven deferred destruction:

<!-- - Assign a monotonically increasing submission serial to each graphics queue
  submission and associate it with the corresponding frame fence. -->
<!-- - After waiting for a frame fence, advance the completed submission serial.
  Graphics queue ordering means completion of serial N also completes all
  earlier submissions on that queue. -->
- When an object, definition, mesh, material, descriptor, buffer, or texture
  becomes unused, stop referencing it immediately but move its graphics-owned
  resources into a retirement batch tagged with the latest submitted serial.
- Destroy retirement batches only after their serial is completed. Asset IDs
  are not reused, so their backing allocations can use the same mechanism.
- Allow old and new definition generations to coexist while work using the old
  generation remains in flight. Allocation keys must include the definition
  version/generation (or use opaque allocation handles) rather than relying
  only on object/renderable/frame indices.
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
