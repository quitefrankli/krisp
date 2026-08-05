#version 450

layout(location = 0) out vec2 frag_tex_coord;

void main()
{
	if (gl_VertexIndex == 0)
	{
		gl_Position = vec4(-1.0, 3.0, 0.0, 1.0);
		frag_tex_coord = vec2(0.0, -1.0);
	}
	else if (gl_VertexIndex == 1)
	{
		gl_Position = vec4(3.0, -1.0, 0.0, 1.0);
		frag_tex_coord = vec2(2.0, 1.0);
	}
	else
	{
		gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
		frag_tex_coord = vec2(0.0, 1.0);
	}
}
