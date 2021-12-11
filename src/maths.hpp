#pragma once

#include <glm/vec3.hpp>


namespace Maths
{
	constexpr float PI = 3.14159265359f;

	inline float deg2rad(float deg)
	{
		return PI * deg / 180.0f;
	}

	inline float rad2deg(float rad)
	{
		return rad * 180.0f / PI;
	}

	struct Ray
	{
		Ray(glm::vec3 origin_, glm::vec3 direction_) : origin(origin_), direction(direction_) {}
		Ray(float&& oX, float&& oY, float&& oZ, float&& dX, float&& dY, float&& dZ) :
			origin(oX, oY, oZ), direction(dX, dY, dZ)
		{}
		glm::vec3 origin;
		glm::vec3 direction;
		float length = 1.0f;
	};
};