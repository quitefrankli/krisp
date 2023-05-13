#version 450
#extension GL_ARB_separate_shader_objects : enable

// note that the fragment shader receives the input as interpolated values
layout(location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(1.0, 1.0, 1.0, 1.0);
}