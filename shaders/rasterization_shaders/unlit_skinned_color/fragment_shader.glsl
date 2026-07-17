#version 450

#include "../../library/library.glsl"

layout(location = 0) out vec4 out_color;

layout(set=RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, binding=RASTERIZATION_MATERIAL_DATA_BINDING) buffer MaterialDataBuffer
{
	MaterialData data;
} mat_data;

void main()
{
	out_color = vec4(mat_data.data.diffuse, 1.0);
}
