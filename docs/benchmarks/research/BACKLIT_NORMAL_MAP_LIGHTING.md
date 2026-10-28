# Backlit normal-map lighting

Date: 2026-10-22

The saved camera reproduced the reported black-and-white pattern on a wall
facing away from a point light. The source scene used one light at `(0, 12, 2)`,
intensity 800, with shadow casting disabled. The camera was at
`(6.425534, 9.539528, 4.726243)`, looking at
`(5.813729, 9.346823, 3.436994)`. Exposure was 0 EV.

The point-light loop rejected lights only against the normal-map shading
normal. Normal texels could therefore admit illumination through a surface
whose base normal faced away from the light. This was separate from missing
normal mips and did not require shadow sampling.

The fix passes base and shading normals separately into
[`evaluate_gltf_point_lights`](../../../shaders/library/library.glsl). Both must
face the light for a direct contribution. The BRDF still uses the shading
normal; shadow bias and receiver-plane projection use the base normal. All
four lit raster material variants pass the appropriate normals, including
double-sided orientation and skinned materials.

This follows the distinction between geometric and shading normals when
classifying reflection and transmission described in
[PBRT's BSDF discussion](https://pbr-book.org/3ed-2018/Materials/BSDFs).

## GPU reproduction

A Release GPU probe renders the actual material shader and highlights fragments
with nonzero direct illumination despite a nonpositive base-normal/light dot
product. It fails when highlighted pixels exceed 0.01% of the fixed test crop.
The initial default-view probe failed at 0.498%; the saved view exposed the
same issue much more strongly.

The saved-view before/after capture used the same mesh, textures, camera, light,
and exposure. A temporary shader reconstructed the previous normal handling
for comparison; compiled production shaders were restored afterward. The
backlit-direct mask covered 21.495% of the crop before the fix and 0% afterward.
The corrected lit capture no longer shows the reported speckles on this wall.
The wall remains dark because its direct point-light contribution is zero;
this change does not add indirect illumination.

Evidence: `build/wall-lighting-investigation/20261022T151428-fa927a/`:

- `pre-fix/capture.png`: reproduced original artifact.
- `baseline/capture.png`: corrected production shader.
- `pre-fix-backlit-direct-mask/capture.png` and
  `backlit-direct-mask/capture.png`: diagnostic comparison.
- `results.json`: pixel counts and shader hashes.

The existing CPU point-light tests cover light packing and projections, not
fragment shading. The GPU diagnostic is the regression check for this issue;
copying the GLSL equations into a CPU test would not exercise the failing path.

Both Debug and Release raster shaders compiled successfully, and all six Debug
test suites passed. A further Release render with one shadow-casting light also
passed the zero-backlit-contribution diagnostic; its evidence is in
`build/wall-lighting-investigation/20261022T151803-41c005/`. The retained probe
is `build/wall-lighting-investigation/probe.py`; it accepts
`backlit-direct-mask` and `--shadowed backlit-direct-mask baseline` for these
checks. `KRISP_WALL_CAMERA_SAVE` selects a saved camera in a detached diagnostic
cache without changing production assets or the save.
