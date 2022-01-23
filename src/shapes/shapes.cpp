#include "shapes.hpp"


Square::Square()
{
	vertices = 
	{
		// notice how the position has +Y going up and -Y going down
		// while the UV has +V going down and -V going up
		// this is because we are not flipping the textures when we load them
		// instead we are loading them in, in read order (as opposed to opengl)

		// our coordinate system is:
		// +X = right
		// +Y = up
		// +Z = out of screen
		// rotation is clockwise in the direction of the axis

		// {pos, color, texCoord, normal}
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}, // bottom left
		{{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}, // bottom right
		{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top right
		{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top left
	};

	indices = 
	{
		0, 1, 2, 2, 3, 0
	};
}

Triangle::Triangle()
{
	throw std::runtime_error("not used");
	vertices = 
	{
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, 
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}
	};
}