## Mesh files

Mesh resources should be binary glTF (`.glb`) files placed in the meshes
resource directory. A mesh file must contain at least one scene with at least
one mesh node. Krisp loads the default scene, or the first scene when no default
is specified, and creates one loaded mesh for each mesh node in that scene.

Each mesh primitive must provide:

* `POSITION`;
* `NORMAL`, unless normal generation is enabled; and
* matching vertex counts for all supplied attributes.

Triangle primitives are preferred. Other supported glTF primitive modes are
converted to triangles by the default loader. Textured materials should provide
`TEXCOORD_0`. Normal-mapped materials require `TEXCOORD_0` and should provide
valid `TANGENT` data; tangents are only generated when that loader option is
enabled.

### Skinned meshes

A mesh node becomes skinned when it references a glTF skin. The skin must
contain at least one joint. If inverse-bind matrices are supplied, they must be
float `MAT4` values with one matrix per joint.

Every skinned primitive must provide `JOINTS_0` and `WEIGHTS_0`, with counts
matching `POSITION`. Krisp supports a maximum of four bone influences per
vertex; additional `JOINTS_n` or `WEIGHTS_n` sets are rejected. Give every
joint a unique, non-empty name if the skeleton will use separately imported
animations.

Loading a skinned mesh creates its skeleton and attaches that skeleton to the
resulting object. Animation files are then imported against this skeleton using
the compatibility rules below.

## Animation files

Animation resources must be glTF (`.gltf`) or binary glTF (`.glb`) files placed
in the animations resource directory. They may be separate from the mesh file,
but must include a skin describing the skeleton targeted by their clips. Each
file must contain:

* at least one animation clip;
* at least one skin; and
* exactly one skin compatible with the target object's skeleton.

A skin is compatible when it has the same number of joints as the target
skeleton loaded from the skinned mesh, every joint has a unique non-empty name,
and the complete named parent hierarchy matches. Joint array order may differ
because joints are mapped by name. Non-joint helper nodes between joints are
ignored when comparing the hierarchy.

Animation channels for nodes outside the compatible skin are ignored. Clip
names should be unique and descriptive so applications can select them
reliably.

Compatibility does not compare bind poses, inverse-bind matrices, bone
lengths, mesh weights, or humanoid semantics. Krisp maps animation tracks
directly to joints; it does not perform proportional or semantic retargeting.
Rigs with matching names and hierarchy but different proportions may therefore
animate incorrectly.
