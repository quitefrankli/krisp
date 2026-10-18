# Resources

Pass resource-relative names without `..` components. For direct resource
registration, a bare filename uses the split layout: model/document files
resolve under `meshes/`, images under `textures/`, and `.kasset` packages under
`kassets/`. A prefixed resource-relative path such as
`animations/motion.glb` or `kassets/collection.kasset` selects its category
explicitly. `ResourcePaths` is the static facade for resolving engine and app
resources and configuration. App resources, configuration, and shaders take
precedence over engine defaults. Precomputed default environment lighting has
its own engine path.

`EnginePaths` groups the engine's default resources, shaders, configuration,
and precomputed environment lighting. In local and editable builds, the
compiled library points these fields at the engine checkout's
`resources/default`, `build/debug/shaders`, `configs/default.yaml`, and
`build/debug/tools/default_environment.krisp-ibl`. The paths are compiled into
the engine centrally, so a consumer does not need engine build macros. A
packaged build uses `share/krisp` paths when explicitly configured for package
data; with development paths disabled, its default is the executable-relative
`krisp-runtime` directory.

`RuntimePaths` combines `app_resources`, `app_config`, `EnginePaths engine`, and
`writable_data`. Call `Config::init(app_name)` before creating the engine to use
default paths, or pass custom `RuntimePaths` to `Config::init` for a custom
layout. App resources default beside the executable; internal bundled apps set
`app_resources` to their source `resources/<app_name>` directory. External
consumers of an editable engine package need only set their own
`app_resources`; the engine supplies its compiled defaults. App-specific
configuration and writable data use the XDG directories. Writable data is
grouped under `$XDG_DATA_HOME/krisp/<app_name>/`, defaulting to
`~/.local/share/krisp/<app_name>/`. Saves, logs, recordings, and screenshots
use subdirectories there. Configuration remains under
`$XDG_CONFIG_HOME/<app_name>/`. `ResourcePaths::paths_for_executable` provides
these defaults independently of the process working directory.

The engine no longer stages, copies, or symlinks engine resources into a
runtime directory. Locally managed asset collections remain in their
respective app directories and are ignored by Git. `Utility` continues to
provide logging, timing, and writable-save helpers; resource and configuration
lookup belongs to `ResourcePaths`.

The Conan package includes default skybox textures, a neutral `checkerboard.png`
texture, configuration, compiled
raster shaders, and precomputed environment lighting. Sample models, animations,
and unrelated asset collections remain application resources. See
[the Conan workflow](CONAN.md) for editable and packaged builds.

## Asset registry

An engine's `AssetRegistry` owns registered aliases and lazy CPU decode caches
for the engine session. A loose resource can be registered with
`load_asset(source)`; its returned descriptors need no authored names. Optional
`AssetLoadOptions::exports` aliases selected document parts, while
`AssetLoadOptions::name` aliases a loose image. `AssetRegistry::describe_source`
can enumerate selectors when callers want to author document aliases. A
`.kasset` package requires explicit names in its index for every importable
part, including dependency images. Subsequent access uses returned typed
handles or explicit aliases; filesystem paths are used only when registering
or restoring a source.
Package descriptors include a member path and selector, so several documents
can be addressed inside one package.

Registration defaults to lazy decoding; `AssetLoadOptions::eager` decodes all
exports into CPU resources immediately. Neither mode uploads GPU resources.
The registry exposes individual meshes, materials, textures, skeletons, and
animations plus assembly exports for selected scenes. Acquiring or instantiating
resources adopts their shared CPU payloads into existing engine stores;
graphics uploads them when used. Model assembly instantiation creates fresh
runtime skeletal state and can bind skinned renderables to a supplied target
skeleton. Animation binding likewise takes an explicit compatible skeleton.
GUI texture previews acquire owning material handles on the game side and pass
those handles to graphics for upload and display. Graphics does not access the
asset registry.

The registry survives ECS resets, while ECS-owned entities and skeleton poses
do not. Reuse registrations after reset and instantiate the required runtime
state again. Unloading a registered source removes its names and cache
ownership; acquired handles and runtime stores keep any shared payloads alive.
See [Kasset assets](KASSET.md) for package format and registry examples.

## Scene state

Scene saves include persistent rigid-body definitions, enabled and active state,
and linear/angular velocities. Bodies are rebuilt from the restored entity
transforms before app state is restored. Transient bodies and transformations
are excluded. Pair-specific contact restitution is saved with persistent bodies.

Applications implement `IApplication::serialize_scene` and `deserialize_scene`
for gameplay state. Store object references as serialized `ObjectID` values;
display names are editable labels. `deserialize_scene` runs after engine/ECS
restoration, and `on_scene_loaded` then refreshes transient input and UI state.
The save schema is developmental; older saves are not retained as a compatibility
format.

## Models

Models must be glTF (`.gltf`) or binary glTF (`.glb`); `.glb` is preferred for
self-contained assets. A loose document can be registered without names; its
returned descriptors can be passed directly to typed registry operations.
Optional export aliases can name only selected parts. A `.kasset` package
requires an explicit index name for every importable part. An assembly
descriptor selects one scene; the default assembly follows the document's
default scene, or scene zero when none is declared. Assembly instantiation
returns one loaded mesh group per mesh node. A scene without mesh nodes returns
an empty result with a warning. Embedded animations are ignored by model
assembly decoding with a warning. Strict mode promotes loader-generated
warnings to errors.

Import converts glTF's right-handed coordinates to Krisp's left-handed basis,
including attributes, winding, node transforms, skin bind data, and animation
tracks. UVs and image pixels retain their glTF orientation.

### Geometry

Every primitive needs `POSITION` and matching counts for each imported
attribute. Missing normals are generated by default; disabling generation makes
`NORMAL` mandatory. Triangles are accepted directly, while triangle strips and
fans are converted to triangles by default. Other primitive modes are rejected.

A textured primitive needs `TEXCOORD_0`; other coordinate sets and per-texture
coordinate selection are unsupported. Normal-mapped primitives also need a
valid tangent basis. Krisp preserves valid float `TANGENT` data or generates it
with MikkTSpace in the default loader. Invalid authored tangents are rejected.

### PBR materials and textures

Krisp imports core glTF metallic-roughness materials with glTF defaults for
`baseColorFactor`, `metallicFactor`, `roughnessFactor`, `emissiveFactor`,
`alphaMode`, `alphaCutoff`, and `doubleSided`. `OPAQUE`, `MASK`, and `BLEND`
alpha modes are supported. `MASK` discards fragments below `alphaCutoff`
(default `0.5`); the cutoff is ignored for the other modes. `BLEND` uses
straight-alpha source-over blending. Double-sided materials render both faces
and reverse the shading normal on back faces. `MASK` materials cast matching
cutout shadows. `BLEND` materials and renderables using per-renderable unlit
shading do not cast shadows.

The following optional maps are supported:

- `baseColorTexture`, sampled as sRGB;
- `metallicRoughnessTexture`, sampled linearly with roughness in G and metallic
  in B;
- `normalTexture`, sampled linearly and including `normalTexture.scale`; and
- `emissiveTexture`, sampled as sRGB.

Texture values modulate their corresponding factors. Emissive RGB is the
component-wise product of `emissiveFactor` and `emissiveTexture`; the texture's
alpha channel is ignored. Emissive output is surface self-illumination only; it
does not emit light into the scene or add bloom. Missing maps retain the
factor-only result. Core glTF does not define emissive strength above one, and
material extensions including `KHR_materials_emissive_strength` are rejected.

Core glTF images must be PNG or JPEG, supplied by external URI, data URI, or GLB
buffer view. `MSFT_texture_dds` may select a DXT5/BC3 DDS image instead, including
external URI, data URI, and GLB `image/vnd-ms.dds` buffer-view sources. Its
texture extension must be declared in `extensionsUsed`. A valid PNG/JPEG core
source is required as the fallback unless the extension is also declared in
`extensionsRequired`. Other DDS formats remain unsupported.

Samplers use linear texel filtering and either repeat or clamp-to-edge
addressing, with the same addressing on both axes. `LINEAR`,
`LINEAR_MIPMAP_NEAREST`, and `LINEAR_MIPMAP_LINEAR` minification are supported;
an omitted minification filter defaults to trilinear mip filtering. Authored DDS
mip chains are uploaded and sampled. PNG/JPEG images remain single-level, so a
mip filter samples their only available level. Nearest texel filtering, mirrored
repeat, and mixed-axis addressing are rejected.

Unsupported features reachable from the selected scene fail the import with a
specific error. Unsupported unused materials, images, textures, or samplers
produce warnings instead, or errors in strict mode. Occlusion textures,
non-zero per-texture `texCoord` selection, and material or texture extensions
other than `MSFT_texture_dds` are rejected. Alpha-to-coverage is not implemented;
masked edges use ordinary fragment discard. Transparent primitives are sorted
per renderable, not per triangle, so intersecting or cyclic transparent
geometry may not composite perfectly.

Standalone textures resolve from the `textures` directories. PNG/JPEG images
are decoded to single-level RGBA8; Krisp does not generate mipmaps. DXT5/BC3 DDS
is also supported and retains a valid authored mip chain; other DDS formats,
cubemaps, and volume textures are rejected.

### Base-color overlays

The Material Editor can append texture overlays to an existing PBR base-color
texture on a mesh with UVs. Each overlay has a UV centre, scale, rotation, linear
RGB tint, and opacity. `GameEngine::composite_renderable_base_color()` exposes
the same operation to consumers. The immutable recipe retains the original
texture and all overlay sources; repeated applications append to a flat stack.
There are at most 64 total layers, including the base texture.

Composition uses premultiplied source-over blending in linear colour and stores
the result in a single-mip sRGB texture at the base texture's dimensions. PBR
sampling converts it back to straight colour before applying the base-colour
factor to the complete composition. The base texture's sampler is preserved.
Metallic-roughness, normal, emissive, alpha mode, and other material properties
are unchanged. Overlay alpha contributes to masked and blended material alpha.

`Apply Overlay` appends the draft overlay. Ordinary material edits preserve the
stack; replacing or clearing the base-color texture removes it. Scene saves
retain recipes and source references for both generated and imported meshes.

### Environment lighting

The default skybox is both the visible background and the source of rasterized
image-based lighting. `GameEngine::spawn_cubemap()` loads six single-level RGBA8
images in `right`, `left`, `top`, `bottom`, `front`, `back` order, corresponding
to the `+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z` cubemap layers. Exactly one active
cubemap may provide environment lighting; a scene without one receives a black
neutral environment.

The graphics engine uses those sRGB faces to produce linear diffuse irradiance,
a complete roughness-prefiltered specular mip chain, and a BRDF integration
lookup texture. Krisp's default environment is precomputed into a versioned
`.krisp-ibl` asset by an incremental Meson build target and shipped in the engine
runtime. `spawn_cubemap()` selects it automatically for engine-default faces.
Runtime validates that asset
against the decoded source faces and processor settings before uploading it.
Cubemaps without an associated precomputed asset use synchronous processing as
a fallback. The current source is the default LDR skybox; custom precomputed
cubemaps and true-HDR environment resources remain future work.

### Skinning

A mesh node is skinned when it references a non-empty glTF skin. Supplied
inverse-bind matrices must be float `MAT4` values with one matrix per joint.
Each skinned primitive needs `JOINTS_0` and `WEIGHTS_0`, matching `POSITION`.
Krisp supports four influences per vertex and rejects additional joint or weight
sets.

Loading creates skeletal state and returns its `SkeletonID` with the unattached
renderables. The caller binds that skeleton to each renderable attachment;
several attachments may share one skeleton. Joints need unique, non-empty names
only when separately imported animations will target the skeleton.

Pass a target `SkeletonID` when instantiating an assembly containing additional
skinned meshes for an existing rig. The source skin must have the same named
joint hierarchy and inverse-bind poses. Krisp remaps `JOINTS_0` when joint
order differs, reuses the target skeleton, and avoids redundant skeletal
state. Attach those renderables to the same object and target skeleton so they
share both the character's world transform and animated pose.

## Animations

Animation exports select a clip and skin from a `.gltf` or `.glb` document.
The selected skin must be compatible with the target skeleton. A compatible
skin has the same joint count, unique non-empty joint names, and the same named
parent hierarchy. Joint array order may differ; non-joint helper nodes are
ignored during hierarchy comparison.

Translation, rotation, and scale channels support linear, step, and cubic-spline
interpolation. Channels outside the compatible skin are ignored. Clips without
supported joint channels are skipped, and loading fails if no usable clips
remain.

Compatibility does not compare bind poses, inverse-bind matrices, bone lengths,
mesh weights, or humanoid semantics. Krisp maps tracks by joint name and does
not perform proportional or semantic retargeting, so structurally compatible
rigs with different proportions may animate incorrectly.
