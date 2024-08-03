#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../../library/library.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
	out_color = STENCIL_COLOR;
}