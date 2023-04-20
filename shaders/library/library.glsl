#include "../../shared_code/shared_data_structures.glsl"

const float AMBIENT_STRENGTH = 0.03;
const float DIFFUSE_STRENGTH = 0.9;
const float SPECULAR_STRENGTH = 0.5;
const float EMISSIVE_STRENGTH = 1.0; // it's high because for now only light sources use this value