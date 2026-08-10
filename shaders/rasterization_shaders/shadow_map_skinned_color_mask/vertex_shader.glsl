#version 450

#include "../../library/library.glsl"

layout(location=0) in vec3 in_position;
layout(location=3) in vec4 bone_ids;
layout(location=4) in vec4 bone_weights;
layout(location=0) out vec3 world_pos;

layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;
layout(set=RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET, binding=RASTERIZATION_BONE_DATA_BINDING) buffer BoneDataBuffer
{
	Bone data[];
} bone_data;

void main()
{
	const mat4 skin_matrix = bone_data.data[int(bone_ids.x)].final_transform * bone_weights.x
		+ bone_data.data[int(bone_ids.y)].final_transform * bone_weights.y
		+ bone_data.data[int(bone_ids.z)].final_transform * bone_weights.z
		+ bone_data.data[int(bone_ids.w)].final_transform * bone_weights.w;
	world_pos = (object_data.data.model * skin_matrix * vec4(in_position, 1.0)).xyz;
	gl_Position = vec4(world_pos, 1.0);
}
