# Performance Notes

Record performance-sensitive implementation choices here. Include measured
results when available; otherwise distinguish expected costs from measurements.

## Point-light shadow filtering

Rasterized point-light shadows use 16-tap Poisson-disc PCF in
`shaders/library/library.glsl`. The helper is shared by the color, texture,
skinned-color, and skinned-texture pipelines.

### Cost

- Each lit fragment performs 16 cubemap depth samples. These texture reads are
  expected to dominate the filter cost.
- Moving from 9 to 16 taps increased shadow texture reads and per-tap arithmetic
  by 78%. This is a theoretical increase, not a frame-time measurement.
- Each tap also normalizes its lookup direction and performs receiver-plane
  depth correction. This avoids concentric self-shadowing on large flat
  receivers, at additional arithmetic cost.
- Textured materials use the geometric normal for shadow correction. Normal-map
  perturbations remain limited to lighting and do not destabilize shadow depth.

### Tuning

- Reducing the Poisson sample count is the most direct quality/performance
  tradeoff. Keep the loop bound and visibility divisor synchronized with the
  kernel size.
- `texel_size` controls a one-texel filter radius relative to the cubemap face
  resolution. Increasing it softens edges but makes undersampling more visible.
- If profiling identifies receiver-plane arithmetic as significant, calculate
  loop-invariant values outside the loop and assess whether normalization can
  be avoided without reintroducing artifacts.

No GPU timing measurements have been recorded yet.

## Skeletal animation cross-fades

Cross-fades retain one local transform snapshot per bone for the transition and
evaluate the target pose into a temporary per-bone pose each update. This adds
linear CPU work and temporary storage proportional to bone count only while a
fade is active. No measurements have been recorded.
