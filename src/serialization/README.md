# Scene Save and Load

`GameEngine::save_scene` and `GameEngine::load_scene` persist the game-side
scene as YAML. The game scene and ECS are the source of truth; graphics state
is rebuilt from them after loading.

The main implementation is in
[`game_engine.cpp`](../game_engine.cpp). Generic YAML traversal lives in
[`serializer.hpp`](serializer.hpp), while imported-resource identity is
tracked by [`resource_provenance.hpp`](resource_provenance.hpp).

## Save files

`SaveFileStore` validates a user-facing save name and maps it to
`<saves-directory>/<name>.yaml`. Names cannot be empty, contain a path
separator, or include an extension.

The document has five top-level sections:

```yaml
engine: {}
camera: {}
materials: []
objects: []
ecs: {}
```

| Section | Contents |
| --- | --- |
| `engine` | Pause and camera-input settings. |
| `camera` | Camera transform, mode, and projection state. |
| `materials` | Scene-owned colour and texture materials that cannot be reconstructed through import provenance. |
| `objects` | Registered scene objects, their transforms, hierarchy, bounds, and renderables. |
| `ecs` | Persistent ECS components and systems associated with scene entities. |

Transient editor state such as the active gizmo selection is not saved.
The gizmo is deselected before serialization so that its temporary parent
relationship cannot enter the scene hierarchy.

## Serialization flow

Saving proceeds in this order:

1. Resolve and validate the destination through `SaveFileStore`.
2. Deselect the gizmo.
3. Serialize engine and camera state.
4. Find every material referenced by a scene object.
5. Serialize non-imported materials.
6. Serialize each registered object.
7. Serialize all persistent ECS systems.
8. Emit YAML to `<save>.yaml.tmp`, then rename it to `<save>.yaml`.

Writing through a temporary file prevents an interrupted write from replacing
the previous save with a partially written document.

### Objects and hierarchy

`Object::serialize` writes:

- the registered serialization type and stable `ObjectID`;
- name, visibility, world transform, relative transform, and bounds;
- `parent_id`, or YAML `null` for a root object;
- every renderable and its pipeline settings.

Parents are represented by IDs rather than nested YAML. This allows objects to
be constructed first and hierarchy links to be restored in a separate pass.

Only types known to `TypeRegistry` may be saved or loaded. `Object::deserialize`
also advances the runtime `ObjectID` counter beyond the restored ID so newly
spawned objects do not reuse it.

### Runtime resources and provenance

Runtime mesh, material, skeleton, and animation IDs are not stable across
processes. Imported glTF/GLB resources therefore use
`ImportedResourceProvenance`, which records their source path and relevant
scene, node, primitive, material, skin, texture, or animation indices.

For an imported renderable, the object stores a `mesh_source` record instead
of relying on its runtime `MeshID`. Imported materials are restored with the
owning primitive and are omitted from the top-level legacy material table.
Imported skeletons and animations use the same provenance principle in their
ECS serialization.

Non-imported resources use their saved IDs:

- colour materials store their lighting values;
- texture materials store their source and texture semantic;
- built-in or otherwise retained meshes use their runtime ID and must still
  exist in `MeshSystem` when loading.

## Deserialization flow

Loading has a validation phase followed by a game-side restore:

1. Read and parse the YAML document.
2. Validate all object types and reject duplicate object IDs.
3. Clear game objects, persistent ECS state, resource tracking, and provenance.
4. Import each referenced model once per source path and glTF scene.
5. Construct and deserialize every object, resolving its renderables.
6. Restore parent relationships.
7. Deserialize ECS systems, followed by engine and camera state.
8. Invoke `IApplication::on_scene_loaded`.
9. Publish the restored state at the next completed game-update boundary.

Loading does not pause or command the graphics thread. Graphics may continue to
render its previously accepted immutable frame while restoration runs. Once
the next completed frame is published, graphics accepts the coherent restored
scene and reconciles membership and versioned definitions.

## `Serializer` and `Deserializer`

`Serializer` builds a shared YAML document through lightweight views returned
by `map`, `sequence`, `append_map`, and `append_sequence`. Scalar writes are
restricted by concepts to supported boolean, numeric, and string-like values.

`Deserializer` owns the parsed YAML document through a shared pointer. Child
and sequence views therefore remain valid for the lifetime of any related
view. Reads:

- require the expected mapping, sequence, scalar, or null kind;
- range-check integral conversions;
- include a document path such as `$.objects[3].renderables[0]` in errors;
- translate YAML conversion failures into `SerializationError`.

Domain helpers in `serialization_helpers.hpp` serialize common math values
such as vectors and transforms without exposing YAML details to scene types.

## ECS persistence

`ECS::serialize` and `ECS::deserialize` delegate to the persistent component
systems in a fixed order:

1. clickable;
2. hoverable;
3. lights;
4. colliders;
5. physics;
6. animation;
7. skeletal data;
8. skeletal animation;
9. tiles.

Transient colliders are deliberately excluded from persistence and are
registered again when the scene resets.

## Adding serializable state

For a new object type:

1. Implement matching `serialize` and `deserialize` methods.
2. Give the type a stable serialization name.
3. Register its factory in `TypeRegistry`.
4. Add a round-trip test that exercises its meaningful state.

For a new ECS subsystem, add matching methods and call them from both
`ECS::serialize` and `ECS::deserialize` in dependency-safe order.

For a new imported resource type, store stable source provenance rather than a
runtime ID, register that provenance when importing, and resolve it before
graphics spawn commands are queued.

## Failure behavior

Parsing and initial object type/ID validation happen before the current scene
is reset. Errors found later—such as a missing resource, invalid imported
primitive, missing parent, or invalid ECS reference—raise
`SerializationError` after reset has begun. The pause guard resumes graphics,
but the game-side scene may contain only the successfully restored prefix.
Callers must report the failure and should not treat the scene as successfully
loaded.
