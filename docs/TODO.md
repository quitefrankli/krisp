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

Animation updates and render-thread pose snapshots are synchronized by the
`SkeletalComponent` pose mutex. However, `get_bones()` still returns a mutable
vector reference, so callers can modify cached `Maths::Transform` components
without taking that mutex.

Replace direct mutable access with explicit read/write pose operations whose
lock lifetime covers the complete operation. This should include animation
updates, IK, editor manipulation, deserialization, and tests. Once all callers
use those operations, make the bone storage private without a mutable-reference
escape hatch.
