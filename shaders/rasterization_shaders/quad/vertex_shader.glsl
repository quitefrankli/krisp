#version 450

#include "../../library/library.glsl"

layout(location=0) out vec2 frag_tex_coord;

void main()
{
	// we are only using 3 vertices so it doesn't really matter if it's not efficient
	if (gl_VertexIndex == 0)
	{
		gl_Position = vec4(-1.0f, 3.0f, 0.0f, 1.0f);
    	frag_tex_coord = vec2(0.0, -1.0);
	} else if (gl_VertexIndex == 1)
	{
		gl_Position = vec4(3.0f, -1.0f, 0.0f, 1.0f);
		frag_tex_coord = vec2(2.0, 1.0);
	} else
	{
		gl_Position = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
		frag_tex_coord = vec2(0.0, 1.0);
	}
}