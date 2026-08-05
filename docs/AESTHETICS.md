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
