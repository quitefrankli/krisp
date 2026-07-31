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
