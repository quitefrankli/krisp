# Visual Aesthetics

## Tone mapping

Krisp renders scene geometry, transparency, overlays, and particles in linear
high-dynamic-range colour. Manual exposure is applied in EV stops before an
ACES-inspired fitted tone-mapping curve. The final presentation attachment
encodes the tone-mapped result as sRGB; editor GUI content is composed afterward
so exposure does not change its appearance.

The curve is Stephen Hill's fit of the ACES reference curve, commonly known
through Krzysztof Narkowicz's implementation. It provides a deliberate filmic
highlight roll-off, but it is not a full ACES colour-management pipeline: Krisp
does not currently perform ACES input/output device transforms or work in an
ACES-defined colour space. Refer to the feature as **ACES-inspired tone
mapping**, not ACES compliance.

Exposure is scene-authored presentation state. `0 EV` is the neutral default;
the editor permits adjustment from `-10 EV` to `+10 EV` in `0.1 EV` increments.

## Rasterized PBR lighting

Krisp's PBR combines direct illumination from the active point light with
image-based lighting derived from the visible default skybox. At startup the
six sRGB skybox faces are converted to linear diffuse irradiance, a
roughness-prefiltered specular cubemap, and a split-sum BRDF lookup texture.
There is no constant ambient term or minimum-lighting floor.

The shadow map is a visibility term for direct point-light illumination only.
Fully occluded fragments receive no direct-light contribution, but may still
receive diffuse and specular environment light. The environment contribution
is not attenuated by the point-light shadow map.

Textured materials preserve the same glTF metallic-roughness response as
factor-only materials. The base-colour texture modulates the base-colour factor;
the packed metallic-roughness texture modulates roughness from G and metallic
from B. Base colour is sampled as sRGB while material-property data is sampled
linearly.

Normal maps perturb the interpolated surface normal in tangent space. Authored
or MikkTSpace-generated tangent handedness keeps mirrored UV regions oriented
correctly, and `normalTexture.scale` controls only the tangent-plane components
before the normal is reconstructed. Omitting a map produces the neutral
factor-only result. Existing point-light shadows, HDR rendering, manual
exposure, and ACES-inspired tone mapping apply unchanged to both material paths.

Emissive factors and textures add self-illumination without lighting other
objects. Core glTF opaque, masked, blended, and double-sided surface policies
are supported. Occlusion effects, custom or true-HDR environment resources,
advanced material extensions, and ray-traced lighting remain outside the
current aesthetic contract. The default skybox is LDR, so its derived lighting
cannot contain radiance above the source image's linear `[0, 1]` range. Manual
exposure and ACES-inspired tone mapping still operate on the combined HDR scene.
The fixed-camera PBR proof scene remains the visual reference; framebuffer-golden
tests are not part of its test contract.

## Unlit rendering

An explicitly unlit renderable displays its base-colour factor modulated by its
optional base-colour texture and opacity. It ignores lights, metallic-roughness,
normal maps, and received shadows, and it never enters the shadow-caster pass.
The result remains scene-linear HDR input: exposure, ACES-inspired tone mapping,
and display encoding still apply.

Editor helper geometry, including transform gizmos, collider visualizations,
camera helpers, and light handles, is explicitly unlit. This keeps its authored
colour readable independently of scene lighting. Helpers retain their existing depth
policy: gizmos render on top, while the light handle remains depth tested. The
global Unlit Base Color mode applies the same material interpretation to scene
geometry; explicit helper shading remains unlit in every global debug mode.
