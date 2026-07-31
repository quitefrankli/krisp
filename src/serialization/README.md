# Scene Save and Load

`GameEngine::save_scene` and `GameEngine::load_scene` persist the game-side
scene as YAML. The game scene and ECS are the source of truth; graphics state
is rebuilt from them after loading.

The main implementation is in
[`game_engine.cpp`](../game_engine.cpp). Generic YAML traversal lives in
[`serializer.hpp`](serializer.hpp), while imported-resource identity is
tracked by [`resource_provenance.hpp`](resource_provenance.hpp).

Krisp's save format follows the current architecture only. There is no legacy
object-owned-renderable or entity-level-skeleton migration path; incompatible
older documents are rejected rather than inferred or silently rewritten.

## Save files

`SaveFileStore` validates a user-facing save name and maps it to
`<saves-directory>/<name>.yaml`. Names cannot be empty, contain a path
separator, or include an extension.

The document has four top-level sections:

```yaml
engine: {}
camera: {}
objects: []
ecs:
  renderable_system: []
```

| Section | Contents |
| --- | --- |
| `engine` | Pause and camera-input settings. |
| `camera` | Camera transform, mode, and projection state. |
| `objects` | Registered scene object type, identity, name, and visibility metadata. |
| `ecs` | Persistent component systems, including renderable attachments, skeletons, animation, and bone attachments. |

Transient editor state such as the active gizmo selection is not saved. The
gizmo is deselected before serialization so its temporary parent relationship
cannot enter the scene hierarchy.

## Persistent identities and ownership

`ObjectID`, `RenderableID`, and `SkeletonID` are independent identities.
Object and renderable IDs are restored directly; imported skeleton IDs are
resolved from provenance and may change across loads:

- An object owns gameplay grouping state only. It does not own renderables or
  skeleton state.
- `renderable_system` owns each renderable attachment. Its record includes the
  persistent `RenderableID`, optional group `ObjectID`, local transform,
  instance visibility, draw and pipeline data, resource provenance, and
  optional `SkeletonID` binding.
- The skeletal system independently owns skeleton topology, pose, and animation
  state. Multiple renderable records may reference one skeleton.

Skinned render types must reference a valid skeleton; non-skinned render types
must not carry a skeleton binding. A grouped renderable composes its local
transform and visibility with the referenced object. A standalone renderable
has no object reference and treats its local transform as world-relative.

Object parents are represented by IDs rather than nested YAML. This allows
objects to be constructed first and hierarchy links to be restored in a
separate pass. Runtime identity counters are advanced beyond restored IDs so
new objects and renderables cannot reuse them.

## Serialization flow

Saving proceeds in this order:

1. Resolve and validate the destination through `SaveFileStore`.
2. Deselect the gizmo.
3. Serialize engine and camera state.
4. Serialize registered objects without renderable or skeleton payloads.
5. Serialize persistent ECS systems in dependency-safe order, including
   skeletons before renderable bindings and bone/equipment attachments.
6. Emit YAML to `<save>.yaml.tmp`, then rename it to `<save>.yaml`.

Writing through a temporary file prevents an interrupted write from replacing
the previous save with a partially written document.

Only object types known to `TypeRegistry` may be saved or loaded. Object
serialization writes the registered type, stable `ObjectID`, name, visibility,
and no component payload. Transformations, hierarchy, colliders, and renderable
attachments appear in their ECS system sections.

### Runtime resources and provenance

Runtime mesh, material, skeleton, and animation IDs are not generally stable
across processes. Imported glTF/GLB resources therefore use
`ImportedResourceProvenance`, which records their source path and relevant
scene, node, primitive, material, skin, texture, or animation indices.

An imported renderable record stores mesh and material source provenance
instead of relying on runtime asset IDs. Imported materials are restored with
their owning primitive. Imported skeletons and animations use the same
provenance principle in their ECS serialization.

Renderable meshes and materials without import provenance are currently
rejected when saving. Procedural skeletons and animations are serialized inline
because their complete definitions are ECS-owned.

## Deserialization flow

Loading performs initial document validation, then restores dependencies in
this order:

1. Read and parse the YAML document; require the current-format sections.
2. Validate registered object types and reject duplicate object IDs.
3. Clear game objects, persistent ECS state, resource tracking, and provenance.
4. Import each referenced model once per source path and glTF scene.
5. Construct objects, then restore ECS transformations and hierarchy followed
   by skeleton definitions and independently owned pose state.
6. Restore renderable attachments, resource references, optional object groups,
   and skeleton bindings, then restore animation state.
7. Restore bone attachments and equipment using their exact source
   `RenderableID`, followed by the remaining ECS, engine, and camera state.
8. Advance persistent ID counters, invoke `IApplication::on_scene_loaded`, and
   publish the restored state at the next completed game-update boundary.

Loading rejects a missing source object/renderable/skeleton, duplicate ID,
skinned/non-skinned binding mismatch, or equipment record whose exact source
renderable no longer exists.

Loading does not pause or command the graphics thread. Graphics may continue to
render its previously accepted immutable frame while restoration runs. Once
the next completed frame is published, graphics accepts the coherent restored
scene and reconciles renderable membership and versioned definitions.

## `Serializer` and `Deserializer`

`Serializer` builds a shared YAML document through lightweight views returned
by `map`, `sequence`, `append_map`, and `append_sequence`. Scalar writes are
restricted by concepts to supported boolean, numeric, and string-like values.

`Deserializer` owns the parsed YAML document through a shared pointer. Child
and sequence views therefore remain valid for the lifetime of any related
view. Reads:

- require the expected mapping, sequence, scalar, or null kind;
- range-check integral conversions;
- include a document path such as `$.ecs.renderable_system[3]` in errors;
- translate YAML conversion failures into `SerializationError`.

Domain helpers in `serialization_helpers.hpp` serialize common math values
such as vectors and transforms without exposing YAML details to scene types.

## Adding serializable state

For a new object type, implement matching serialization methods, give it a
stable type name, register its factory in `TypeRegistry`, and add a meaningful
round-trip test. Do not add renderable or skeleton ownership to the object.

For a new ECS subsystem, add matching methods and call them from both
`ECS::serialize` and `ECS::deserialize` in dependency-safe order. For a new
imported resource type, store stable source provenance rather than a runtime ID
and resolve it before restoring dependent attachments.

## Failure behavior

Parsing plus object-type and duplicate-`ObjectID` validation happen before the
current scene is reset. Errors found later, such as a missing imported resource,
raise `SerializationError` after reset has begun. The pause guard resumes
graphics, but the game-side scene may contain only the successfully restored
prefix. Callers must report the failure and must not treat the scene as
successfully loaded.
