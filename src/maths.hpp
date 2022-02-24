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

	struct Plane
	{
		Plane() = default;
		Plane(const glm::vec3& offset_, const glm::vec3& normal_) : offset(offset_), normal(normal_) {}

		glm::vec3 offset;
		glm::vec3 normal;
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

	// applies rotation between vectors assuming start=forward vec
	glm::quat Vec2Rot(const glm::vec3& vec);

	// gets intersection between ray and plane, ASSUMING there is one, use below function if checking is necessary
	glm::vec3 ray_plane_intersection(const Ray& ray, const Plane& plane);

	// it's possible for a ray to be "behind" a plane and pointing away from it
	bool check_ray_plane_intersection(const Ray& ray, const Plane& plane);

	const glm::vec3 up_vec{0.0f, 1.0f, 0.0f};
	const glm::vec3 forward_vec{0.0f, 0.0f, -1.0f};
	const glm::vec3 zero_vec{};
};