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

Krisp's PBR uses direct illumination from the active point light only. It does
not add image-based lighting or retain the former constant ambient term. A
surface outside the light's influence, facing away from it, or fully occluded
by the point-light shadow map may therefore be black. This is an intentional
statement of the currently modelled light transport, not a minimum-lighting
floor or a tone-mapping defect.

The shadow map is a visibility term for direct illumination. Fully occluded
fragments receive no direct-light contribution; Krisp does not add residual
light to soften complete shadow. Indirect illumination should be introduced
explicitly through a later image-based-lighting stage rather than approximated
with material ambient colour or shadow leakage.

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

Environment lighting, emissive and occlusion effects, alpha modes, double-sided
surfaces, advanced material extensions, and ray-traced lighting are outside the
current aesthetic contract. The fixed-camera PBR proof scene is the visual
reference for this stage; framebuffer-golden tests are not part of its test
contract.

## Unlit rendering

An explicitly unlit renderable displays its base-colour factor modulated by its
optional base-colour texture and opacity. It ignores lights, metallic-roughness,
normal maps, and received shadows, and it never enters the shadow-caster pass.
The result remains scene-linear HDR input: exposure, ACES-inspired tone mapping,
and display encoding still apply.

Editor helper geometry, including transform gizmos, collider visualizations,
camera helpers, and light handles, is explicitly unlit. This keeps its authored
colour readable under direct-light-only PBR. Helpers retain their existing depth
policy: gizmos render on top, while the light handle remains depth tested. The
global Unlit Base Color mode applies the same material interpretation to scene
geometry; explicit helper shading remains unlit in every global debug mode.
