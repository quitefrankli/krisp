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

## Stage 1 PBR lighting

Stage 1 PBR uses direct illumination from the active point light only. It does
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