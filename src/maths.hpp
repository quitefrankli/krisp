#pragma once

#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <optional>


namespace Maths
{
	constexpr float PI = 3.14159265359f;
	constexpr float ACCEPTABLE_FLOATING_PT_DIFF = 0.00001f;

	inline float deg2rad(float deg)
	{
		return PI * deg / 180.0f;
	}

	inline float rad2deg(float rad)
	{
		return rad * 180.0f / PI;
	}

	inline bool is_vec3_equal(const glm::vec3& v1, const glm::vec3& v2)
	{
		const auto res = glm::epsilonEqual(v1, v2, ACCEPTABLE_FLOATING_PT_DIFF);
		return res.x && res.y && res.z;
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

	struct Transform
	{
	public:
		const glm::vec3& get_pos();
		const glm::vec3& get_scale();
		const glm::quat& get_orient();
		const glm::mat4& get_mat4();

		void set_pos(const glm::vec3& new_pos);
		void set_scale(const glm::vec3& new_scale);
		void set_orient(const glm::quat& new_orient);
		void set_mat4(const glm::mat4& new_transform);

	private:
		glm::vec3 position = glm::vec3(0.f); // 0b1000
		glm::vec3 scale = glm::vec3(1.f); // 0b0100
		glm::quat orientation = glm::quat_identity<float, glm::highp>(); // 0b0010
		glm::mat4 transform = glm::mat4(1.0f); // 0b0001

		// flags whether the above vars are old
		// if it's old then it should be either recalculated to/from the mat4
		uint8_t is_up_to_date = 0b1111;

		bool is_old(uint8_t bitmask) const { return !(is_up_to_date & bitmask); }
	};

	// assume normalized already
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end);
	// when the axis is already known
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end, const glm::vec3& axis);

	// applies rotation between vectors assuming start=forward vec
	glm::quat Vec2Rot(const glm::vec3& vec);

	// gets intersection between ray and plane, ASSUMING there is one, use below function if checking is necessary
	glm::vec3 ray_plane_intersection(const Ray& ray, const Plane& plane);

	// it's possible for a ray to be "behind" a plane and pointing away from it
	bool check_ray_plane_intersection(const Ray& ray, const Plane& plane);

	bool check_spherical_collision(const Ray& ray, const Sphere& sphere);

	std::optional<glm::vec3> ray_sphere_collision(const Sphere& sphere, const Ray& ray);

	inline float signed_sum(const glm::vec3& v) { return v.x + v.y + v.z; }

	const glm::vec3 right_vec = { 1.0f, 0.0f, 0.0f };
	const glm::vec3 up_vec = { 0.0f, 1.0f, 0.0f };
	const glm::vec3 forward_vec = { 0.0f, 0.0f, 1.0f };
	const glm::vec3 zero_vec = {};
	const glm::vec3 identity_vec = { 1.0f, 1.0f, 1.0f };
	const glm::quat identity_quat = glm::quat_identity<float, glm::highp>();
	enum class Direction
	{
		RIGHT,
		UP,
		FORWARD
	};
};