#version 450

#extension GL_GOOGLE_include_directive : enable

#include "../../library/library.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;
layout(location=0) in vec3 in_world_pos[];
layout(location=1) in vec2 in_tex_coord[];

layout(set=RASTERIZATION_LOW_FREQ_SET_OFFSET, binding=RASTERIZATION_GLOBAL_DATA_BINDING) uniform GlobalDataBuffer
{
	GlobalData data;
} global_data;

layout(location=0) out vec3 out_world_pos;
layout(location=1) out vec2 out_tex_coord;

void main()
{
	for (int face_idx = 0; face_idx < 6; ++face_idx)
	{
		gl_Layer = face_idx;
		for (int vertex_idx = 0; vertex_idx < 3; ++vertex_idx)
		{
			out_world_pos = in_world_pos[vertex_idx];
			out_tex_coord = in_tex_coord[vertex_idx];
			gl_Position = global_data.data.shadow_view_proj_mats[face_idx]
				* vec4(in_world_pos[vertex_idx], 1.0);
			EmitVertex();
		}
		EndPrimitive();
	}
}
