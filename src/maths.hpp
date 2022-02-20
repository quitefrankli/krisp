#pragma once

#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>


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

	struct Sphere
	{
		Sphere() = default;
		Sphere(glm::vec3 origin_, float radius_) : origin(origin_), radius(radius_) {}
		glm::vec3 origin;
		float radius;
	};

	struct TransformationComponents
	{
		glm::vec3 position = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);
		glm::quat orientation; // default init creates identity quaternion
	};

	// assume normalized already
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end);

	const glm::vec3 forward_vec{0.0f, 0.0f, -1.0f};
};