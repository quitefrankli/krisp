# Kasset assets

`.kasset` is a read-only package for existing engine resource formats. It can
hold glTF/GLB documents, images, and their dependencies, then expose selected
parts under authored names. It packages resources; it does not define a new
mesh, material, animation, scene, or ECS format.

## Registering resources

An `AssetRegistry` belongs to an engine session and owns registered names and
lazy CPU decode data. Register each source before accessing its descriptors or
aliases. Loose source paths are resolved only at registration: an unprefixed path
uses the existing split resource layout (`.kasset` from `kassets/`, images from
`textures/`, and other resources from `meshes/`); an explicit category-prefixed
resource path is also accepted. Package member paths are separate from the
source path. After registration, callers use names, not filesystem paths.

Loose resources, including documents and images, can be loaded without
authored names. Registration returns their available descriptors, which can be
passed directly to typed registry operations. For documents,
`AssetLoadOptions::exports` assigns optional aliases to selected parts; a loose
image accepts an optional `AssetLoadOptions::name`. Without an alias, use the
returned descriptor handle. Packages are different: every part exposed by a
`.kasset` index has a mandatory authored name, and all importable parts in each
document member need exports, including dependency images.

```cpp
auto assets = engine.assets().load_asset("meshes/scene.glb");
const auto assembly = AssetRegistry::default_assembly(assets);
auto model = engine.assets().instantiate_model(ecs, assembly);
```

For a named loose image, set `AssetLoadOptions::name`; for a loose document,
`AssetRegistry::describe_source` can enumerate selectors for constructing
aliases. Explicit names are unique within a registry. A source already
registered with different aliases or import options must first be unloaded.
`AssetRegistry::contains(name)` reports only explicit aliases; unnamed
descriptors remain available in the returned `Asset` list.

The registry records alias choices and importer settings in one immutable
registration descriptor shared by provenance records for that source.
Repeating `load_asset` with the same source and options returns its existing
exports without rereading the source; different options are rejected.

The registry exposes individual meshes, materials, images, skeletons, and
animations, as well as assembly descriptors for selected scenes. A mesh
selector identifies its scene/node/primitive; an assembly selects a scene and
instantiates its renderables. A skeleton selects a skin, and an animation
selects a clip and each skin targeted by that clip's joint channels. These
selectors are source indices, not runtime IDs. Loose-document descriptors can
remain unnamed. Package selectors must appear in the index with explicit names
for every exposed part and every dependency image.

`AssetLoadOptions::eager` performs the CPU decoding of exports during
registration. Its default, `false`, records metadata and defers decoding until
an export is acquired or instantiated. Neither mode creates GPU resources.
GPU upload happens when resources are adopted into the engine's runtime stores
and used by graphics. Creating a skeleton or binding an animation is an
explicit runtime operation; an animation needs a compatible target skeleton.
An assembly can receive a target skeleton at instantiation when its skinned
renderables should share existing skeletal state.

The registry remains owned by the engine session across ECS resets. Resetting
an ECS clears runtime entities and skeletal state, not registrations or their
CPU cache. Reuse registered names after a reset, and instantiate skeletons or
models again as needed. `unload_asset(source)` removes that source's names and
registry ownership. Already acquired handles and runtime stores keep their
shared resource data alive for as long as they own it.

Scene saves record each source path and registration descriptor once in
`resources.asset_sources`; each imported resource reference stores its own
package member and selector alongside the shared `source_id`.

## Package layout and format

The package is a 24-byte little-endian header, UTF-8 JSON index, then
uncompressed file payloads. The header is `KASSET\0\0`, a `uint32` version
(`1`), `uint32` flags (`0`), and a `uint64` JSON index length. Version 1 rejects
unknown flags. Index file offsets are absolute offsets from the beginning of
the package. File entries give a canonical package member path, offset, and
size. Export entries give a unique authored name, kind, member file, and
optional integer selectors. Payloads are stored without compression.

The JSON index has this shape; offsets shown are absolute package offsets:

```json
{
  "files": [
    {"path": "model/scene.glb", "offset": 512, "size": 4096},
    {"path": "model/scene.bin", "offset": 4608, "size": 8192}
  ],
  "exports": [
    {"name": "scene.assembly", "kind": "assembly", "file": "model/scene.glb",
     "selector": {"scene": 0}}
  ]
}
```

For example, a compact recipe can name a document assembly and its component
parts independently:

```json
{
  "files": [
    {"path": "model/scene.glb", "source": "scene.glb"}
  ],
  "exports": [
    {"name": "scene.assembly", "kind": "assembly", "file": "model/scene.glb",
     "selector": {"scene": 0}},
    {"name": "scene.mesh.0", "kind": "mesh", "file": "model/scene.glb",
     "selector": {"scene": 0, "node": 2, "primitive": 0}},
    {"name": "scene.material.0", "kind": "material", "file": "model/scene.glb",
     "selector": {"material": 0}},
    {"name": "scene.image.0", "kind": "texture", "file": "model/scene.glb",
     "selector": {"image": 0}}
  ]
}
```

The example is illustrative: a real recipe must include exports for every
importable part enumerated by the document metadata. The packer gathers
external glTF buffer and image dependencies and adds them as package members;
data URIs remain embedded. Package dependencies resolve only within the
package. Absolute paths, network URIs, and paths escaping the package are
rejected; there is no loose-file fallback. Physical member paths are
canonical, slash-separated relative paths. A package can share one dependency
member among multiple documents. Nested packages, compression, and serialized
ECS state are not supported.

For a single source, no recipe is needed:

```sh
python3 tools/pack_kasset.py SOURCE.gltf OUTPUT.kasset
```

Direct inputs support glTF, GLB, PNG, JPEG, and DDS. The packer gathers model
dependencies and generates names for every importable part. Names start with
the source filename stem, resource kind, and selector indices, followed by the
authored name when available. Indices keep duplicate or missing authored names
unambiguous. For example, an unnamed scene is `asset.assembly.0`, and a named
material is `asset.material.0.Steel`. A standalone image is `asset.texture`.
The names are written into the package; runtime packages still reject unnamed
assets. Consumers can use the returned descriptors without knowing these names.

For custom names or multiple sources, use
`tools/pack_kasset.py RECIPE.json OUTPUT.kasset`. The
recipe's `source` paths are resolved relative to the recipe while packing. They
are authoring inputs only; runtime lookup uses the registered package source
and export names. The packer writes sorted members and exports and replaces the
output only after a complete package has been written. Packer dependency,
range, and deterministic-output checks run as the `pack_kasset` Meson test.

## Decode, ownership, and performance

Registration reads metadata to enumerate and validate exports. Decoding a
document caches its complete TinyGLTF model, including its buffers and decoded
dependent images; selectors do not cause per-primitive file reads or partial
document parsing. Image data therefore remains part of the document's cached
CPU footprint even if only one exported primitive is requested. Named meshes
and materials are decoded into immutable CPU resource payloads. The existing
counted stores can adopt those payloads with shared ownership and reuse IDs,
without copying CPU geometry or material data. Texture identities are
source-and-semantic specific: one source image can yield separate handles for
sRGB and linear GPU interpretation, while its encoded/decoded source remains
cached.

This cache is scoped to registered sources in the engine session, not global.
Provenance records share the source's immutable registration descriptor rather
than copying its names and importer settings per resource.
Eager registration moves decode work earlier but does not reduce the amount of
CPU data retained. Package range reads avoid extracting members to disk, while
uncompressed payloads permit direct reads. No asset load-time or memory
benchmark is claimed; see the scoped notes in
[`PERFORMANCE.md`](PERFORMANCE.md).
