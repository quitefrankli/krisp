#version 450

#include "../../library/library.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 bone_ids;
layout(location = 4) in vec4 bone_weights;

layout(location = 0) out vec2 frag_tex_coord;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_BONE_DATA_BINDING) buffer BoneDataBuffer
{
	Bone data[];
} bone_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

void main()
{
	const mat4 skin_matrix =
		bone_data.data[int(bone_ids.x)].final_transform * bone_weights.x +
		bone_data.data[int(bone_ids.y)].final_transform * bone_weights.y +
		bone_data.data[int(bone_ids.z)].final_transform * bone_weights.z +
		bone_data.data[int(bone_ids.w)].final_transform * bone_weights.w;
	gl_Position = global_data.data.proj * global_data.data.view * skin_matrix * vec4(in_position, 1.0);
	frag_tex_coord = in_tex_coord;
}
