# Architecture TODOs

## Encapsulate mutable skeletal poses

Animation updates and render-frame pose snapshots are game-thread confined,
and graphics consumes only copied bone transforms. Bone definitions are now
private and `get_bones()` returns a const reference. However,
`get_bone_local_transform()` still returns a mutable transform reference, so
pose mutation is not expressed through an explicit skeletal-component operation.

Replace direct mutable access with explicit pose operations. This should
include animation updates, IK, editor manipulation, deserialization, and
tests. Once all callers use those operations, remove the remaining mutable pose
reference. Add synchronization only if pose mutation is later moved off the game
thread.

## Add a PBR material and lighting path

Replace the current Blinn-Phong lighting path with a defined physically based
material model, including the supported glTF metallic-roughness inputs and an
image-based-lighting strategy.

## Review GUI and shared-resource synchronization

Document the thread ownership of GUI state and the global mesh/material
registries. Retain locks required for cross-thread access and remove only locks
shown to be unnecessary.

## Remove static Material/Mesh Systems

Replace the process-wide `CountableSystem` registries with engine-owned mesh and
material stores. Preserve explicit shared ownership, stable runtime IDs, and the
retirement handoff to graphics without introducing global accessors.

## Add serialization support for procedurally generated meshes and materials

### Add a cache for procedurally generated meshes and materials
