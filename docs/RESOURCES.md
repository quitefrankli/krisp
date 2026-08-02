## Mesh files

Mesh resources may be glTF (`.gltf`) or binary glTF (`.glb`); `.glb` is
preferred for self-contained assets. Pass a resource-relative filename without
`..` components. Krisp searches `resources/<project>/meshes` first and
`resources/default/meshes` second.

glTF resources are converted from glTF's right-handed coordinate system to
Krisp's left-handed coordinate system during import. The conversion covers
mesh attributes and winding, node transforms, skin bind data, and animation
tracks. Texture coordinates and image pixels retain their glTF orientation.

A mesh file must contain at least one scene. Krisp loads the default scene, or
the first scene when no default is specified, and creates one loaded mesh for
each mesh node in that scene. A selected scene without mesh nodes returns an
empty result with a warning, or raises an error in strict mode.

Each mesh primitive must provide:

* `POSITION`;
* `NORMAL`, unless normal generation is enabled; and
* matching vertex counts for supplied attributes Krisp imports:
  `NORMAL`, `TEXCOORD_0`, `TANGENT`, `JOINTS_0`, and `WEIGHTS_0`.

Triangle primitives are preferred. Other supported glTF primitive modes are
converted to triangles by the default loader. Textured materials should provide
`TEXCOORD_0`. Normal-mapped materials require `TEXCOORD_0` and should provide
valid `TANGENT` data. The convenience `load_model(ecs, filename)` overload
generates or regenerates tangents when needed. When explicit `LoadOptions` are
used, tangent generation is disabled unless `generate_missing_tangents` is set.

### Skinned meshes

A mesh node becomes skinned when it references a glTF skin. The skin must
contain at least one joint. If inverse-bind matrices are supplied, they must be
float `MAT4` values with one matrix per joint.

Every skinned primitive must provide `JOINTS_0` and `WEIGHTS_0`, with counts
matching `POSITION`. Krisp supports a maximum of four bone influences per
vertex; additional `JOINTS_n` or `WEIGHTS_n` sets are rejected. Give every
joint a unique, non-empty name if the skeleton will use separately imported
animations.

Loading a skinned mesh creates independently owned skeletal state and returns
its `SkeletonID` beside the unattached renderables. Callers bind that ID when
they add each skinned renderable attachment. The binding belongs to the
attachment, not to its optional object group, and several renderables may share
the same skeleton pose and animation state. Animation files are imported
against that skeleton using the compatibility rules below.

## Animation files

Animation resources must be glTF (`.gltf`) or binary glTF (`.glb`). Pass a
resource-relative filename; Krisp searches `resources/<project>/animations`
first and `resources/default/animations` second. Animation files may be separate
from the mesh file, but must include a skin describing the skeleton targeted by
their clips. Each file must contain:

* at least one animation clip;
* at least one skin; and
* exactly one skin compatible with the target skeleton.

At least one clip must contain a supported translation, rotation, or scale
channel targeting a joint in the compatible skin. Clips without such channels
are skipped, and loading fails when no usable clips remain.

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
