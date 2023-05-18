#version 450

#include "../../library/library.glsl"


// keep in mind that some types such as dvec3 uses 2 slots therefore we need the next layout location to be 2 indices after
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_tex_coord;
layout(location=3) in vec4 bone_ids;
layout(location=4) in vec4 bone_weights;

layout(location=0) out vec2 frag_tex_coord;
layout(location=1) out vec3 surface_normal;
layout(location=2) out vec3 frag_pos;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_OBJECT_DATA_BINDING) uniform ObjectDataBuffer
{
	ObjectData data;
} object_data;

layout(set=RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET, binding=RASTERIZATION_BONE_DATA_BINDING) buffer BoneDataBuffer
{
	Bone data[];
} bone_data;

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

mat4 get_bone_matrix(float index)
{
	return bone_data.data[int(index)].final_transform;
}

void main()
{
	// this is blending the different transforms together, the reason it works is because...
	// 1. the bone weights are normalized
	// 2. Matrices are linear => Av + Bv = (A + B)v
	mat4 skin_matrix =
		get_bone_matrix(bone_ids.x) * bone_weights.x + 
		get_bone_matrix(bone_ids.y) * bone_weights.y + 
		get_bone_matrix(bone_ids.z) * bone_weights.z + 
		get_bone_matrix(bone_ids.w) * bone_weights.w;
	frag_pos = (skin_matrix * vec4(in_position, 1.0)).xyz;

    surface_normal = (skin_matrix * vec4(in_normal, 0.0)).xyz;
	frag_tex_coord = in_tex_coord;
    gl_Position = global_data.data.proj * global_data.data.view * vec4(frag_pos, 1.0);
}