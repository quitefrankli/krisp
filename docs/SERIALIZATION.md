# Scene Serialization

`GameEngine::save_scene` stores each save as a directory:

```text
<saves>/<name>/
  scene.yaml
  mesh_<id>.dat
  texture_<id>.dat
```

`scene.yaml` contains engine, render settings, camera, object, ECS, and resource
metadata. The game scene and ECS remain the source of truth; graphics state is
rebuilt after loading. The scene format is not versioned during early
development; saved scenes must match the current schema.

The `render_settings` map stores scene-authored presentation state, currently
manual exposure in EV stops.

## Resources

Resources are classified by `ImportedResourceProvenance`:

- Imported resources store their external source and selector fields. A model
  source can select a mesh, material, skeleton, or animation; a texture source
  identifies a standalone image and its semantic. External bytes are not copied.
  Imported-material edits are stored as sparse, flattened overrides against the
  original glTF material. Only user-changed factor, normal-scale, or texture-slot
  fields are persisted, so unspecified fields continue to follow compatible
  updates to the source asset. Each texture slot distinguishes inherited,
  replaced, and explicitly cleared states.
  Imported glTF texture identities use the source image plus texture semantic;
  sampler choice remains part of the PBR slot binding. Each sampler records its
  address mode and whether mip selection is disabled, nearest, or linear.
- Resources without provenance were generated inside Krisp. Generated meshes
  and texture payloads are stored once as `.dat` files; PBR material parameters
  and optional texture references are written directly in YAML. References
  preserve resource sharing.

Generated PBR materials store glTF-native `base_color_factor`,
`metallic_factor`, and `roughness_factor` values, optional base-colour, packed
metallic-roughness, normal, and emissive texture references, `normal_scale`,
emissive factor, alpha policy, and double-sided policy. Texture semantics
preserve the required sRGB or linear interpretation. Standalone BC3 DDS payloads
retain their supplied mip metadata; decoded PNG/JPEG textures are single-mip
because Krisp does not generate mipmaps. The early-development scene format does
not translate the removed ambient, diffuse, specular, emissive, or shininess
fields; saves using that legacy schema are unsupported.

The skybox renderable and its six source texture references remain scene data.
Derived irradiance, prefiltered specular, and BRDF lookup images are rebuilt by
graphics and are never serialized as resource payloads.

The material editor applies an override only to the selected renderable. Other
renderables that originated from the same shared glTF material remain attached
to the unmodified source material. Apply and Clear are explicit operations;
Clear serializes the texture slot's cleared state rather than reverting it to
inheritance. A texture edit is rejected when the selected mesh lacks the UV or
tangent-space data required by that slot.

Mesh files have a magic value, format version, vertex layout, counts, canonical
little-endian vertex fields, and `uint32` indices. Texture files contain the raw
payload described by their YAML dimensions, format, semantic, and mip sizes.
Resource filenames are restricted to direct `.dat` children of the save directory.

The implementation is in
[`scene_resources.cpp`](../src/serialization/scene_resources.cpp); external
identity tracking is in
[`resource_provenance.hpp`](../src/serialization/resource_provenance.hpp).

## Identity and ownership

`ObjectID`, `RenderableID`, and `SkeletonID` are independent. Object IDs are
persistent. Renderables and skeletons receive fresh runtime IDs on load because
graphics may have already observed the old identities and their topology.
Saved-to-runtime maps resolve skeleton bindings, animations, bone attachments,
and equipment references.

Objects contain gameplay grouping metadata. ECS systems own transformations,
colliders, renderable attachments, skeletons, animations, and equipment.
Skinned renderables must reference a skeleton; non-skinned renderables must not.
Transient editor state is omitted.

Persistent renderables store their `shading_mode` as either lit or unlit. Shadow
casting remains an independent authored property, but unlit renderables never
cast at runtime even when the stored `casts_shadow` value is true.

## Save and load

Saving serializes to a staging directory, writes `scene.yaml.tmp`, renames it to
`scene.yaml`, then atomically exchanges the staged directory with the previous
save. A failed save restores the previous directory where possible.

Loading validates object types and duplicate object IDs before resetting the
scene. It then:

1. Imports each referenced external model once and reconstructs generated resources.
2. Restores objects and ECS systems in dependency order.
3. Remaps saved renderable and skeleton relationships to fresh runtime IDs.
4. Restores engine/camera state and publishes the next coherent render snapshot.

Malformed YAML, unsafe resource paths, corrupt binary data, missing external
resources, invalid relationships, and unsupported resource types raise
`SerializationError`. Failures discovered after reset can leave a partially
restored game-side scene; graphics retains its last accepted immutable frame.

## YAML utilities and extensions

`Serializer` creates mapping and sequence views over one YAML document.
`Deserializer` owns the parsed document, validates node kinds and numeric ranges,
and includes paths such as `$.ecs.renderable_system[3]` in errors. Common math
types use `serialization_helpers.hpp`.

When adding state, serialize it in its owning ECS subsystem and preserve
dependency order in `ECS::serialize`/`deserialize`. Give imported resources
stable provenance rather than persisting runtime IDs. Add the lowest-level
round-trip or rejection test that demonstrates the new behaviour.
