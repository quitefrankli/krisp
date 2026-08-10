#version 450

#include "../../library/library.glsl"

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=3) in vec4 bone_ids;
layout(location=4) in vec4 bone_weights;
layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;
layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_BONE_DATA_BINDING) buffer BoneDataBuffer
{
	Bone data[];
} bone_data;
layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

void main()
{
	const mat4 skin_matrix = bone_data.data[int(bone_ids.x)].final_transform * bone_weights.x
		+ bone_data.data[int(bone_ids.y)].final_transform * bone_weights.y
		+ bone_data.data[int(bone_ids.z)].final_transform * bone_weights.z
		+ bone_data.data[int(bone_ids.w)].final_transform * bone_weights.w;
	const mat4 model_skin = object_data.data.model * skin_matrix;
	const vec3 world_position = (model_skin * vec4(in_position, 1.0)).xyz;
	const vec3 world_normal = normalize(transpose(inverse(mat3(model_skin))) * in_normal);
	gl_Position = global_data.data.proj * global_data.data.view
		* vec4(world_position + world_normal * STENCIL_OFFSET, 1.0);
}
