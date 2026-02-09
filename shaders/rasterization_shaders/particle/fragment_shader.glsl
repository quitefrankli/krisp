#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

// Optional texture for the particle
layout(set = RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, 
  	   binding = RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING) uniform sampler2D tex_sampler;

// Material data binding for texture flags
layout(set = RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, 
	   binding = RASTERIZATION_MATERIAL_DATA_BINDING) readonly buffer MaterialDataBuffer
{
	MaterialData data;
} material_data;

void main()
{
	vec4 color = frag_color;
	
	// Check if texture is enabled (texture_flags bit 0)
	if ((material_data.data.texture_flags & 1) != 0)
	{
		vec4 tex_color = texture(tex_sampler, frag_tex_coord);
		// Alpha test - discard transparent fragments
		if (tex_color.a < 0.01)
		{
			discard;
		}
		color *= tex_color;
	}
	else
	{
		// No texture - use a circular gradient for soft particles
		vec2 center = frag_tex_coord - vec2(0.5);
		float dist = length(center) * 2.0;
		float alpha = 1.0 - smoothstep(0.8, 1.0, dist);
		color.a *= alpha;
		
		if (color.a < 0.01)
		{
			discard;
		}
	}
	
	out_color = color;
}
