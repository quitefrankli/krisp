#pragma once

#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <optional>
#include <cmath>


namespace Maths
{
	constexpr float PI = 3.14159265359f;
    constexpr float EULERS_NUMBER = 2.7182818f;
	constexpr float ACCEPTABLE_FLOATING_PT_DIFF = 0.00001f;

	template<class T>
	T SigmoidFunction(T input);

	template<class T>
	T RandomUniform(T min, T max);

	template<class T>
	T RandomNormal(T mean, T stdDev);

	template<class T>
	T lerp(T a, T b, float t);

	constexpr float deg2rad(float deg) { return PI * deg / 180.0f; }
	constexpr float rad2deg(float rad) { return rad * 180.0f / PI; }

	constexpr bool is_vec3_equal(const glm::vec3& v1, const glm::vec3& v2)
	{
		const auto res = glm::epsilonEqual(v1, v2, ACCEPTABLE_FLOATING_PT_DIFF);
		return res.x && res.y && res.z;
	}

	constexpr float absf(float x) { return std::abs(x); }
	constexpr float sqrtf(float x) { return std::sqrt(x); }
	constexpr float sinf(float x) { return std::sin(x); }
	constexpr float cosf(float x) { return std::cos(x); }

	struct Ray
	{
		Ray() = default;
		Ray(glm::vec3 origin_, glm::vec3 direction_) : origin(origin_), direction(direction_) {};
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
		glm::vec3 origin = glm::vec3(0.0f);
		float radius = 0.5f;
	};

	struct Transform
	{
	public:
		Transform() = default;
		Transform(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& orient) :
			position(pos), scale(scale), orientation(orient)
		{
			is_up_to_date_flags = 0b1110;
		}

		Transform(const glm::mat4& mat4) : transform(mat4) 
		{
			is_up_to_date_flags = 0b0001;
		}

		const glm::vec3& get_pos() const;
		const glm::vec3& get_scale() const;
		const glm::quat& get_orient() const;
		const glm::mat4& get_mat4() const;

		void set_pos(const glm::vec3& new_pos);
		void set_scale(const glm::vec3& new_scale);
		void set_orient(const glm::quat& new_orient);
		void set_mat4(const glm::mat4& new_transform);

	private:
		mutable glm::vec3 position = glm::vec3(0.f); // 0b1000
		mutable glm::vec3 scale = glm::vec3(1.f); // 0b0100
		mutable glm::quat orientation = glm::quat_identity<float, glm::highp>(); // 0b0010
		mutable glm::mat4 transform = glm::mat4(1.0f); // 0b0001

		// flags whether the above vars are old
		// if it's old then it should be either recalculated to/from the mat4
		// 1 means it's up to date 0 means it's old
		mutable uint8_t is_up_to_date_flags = 0b1111;

		void set_not_old(uint8_t bitmask) const { is_up_to_date_flags |= bitmask; }
		bool is_old(uint8_t bitmask) const { return !(is_up_to_date_flags & bitmask); }
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

	bool check_ray_rod_collision(const Ray& ray, const glm::vec3& rod_start, const glm::vec3& rod_end, const float radius);
	bool check_ray_rod_collision(const Ray& ray, 
								 const glm::vec3& rod_start, 
								 const glm::vec3& rod_end, 
								 const float radius,
								 glm::vec3& out_intersection);


	std::optional<glm::vec3> ray_sphere_collision(const Sphere& sphere, const Ray& ray);

	inline float signed_sum(const glm::vec3& v) { return v.x + v.y + v.z; }

	const glm::vec3 right_vec = { 1.0f, 0.0f, 0.0f };
	const glm::vec3 up_vec = { 0.0f, 1.0f, 0.0f };
	const glm::vec3 forward_vec = { 0.0f, 0.0f, 1.0f };
	const glm::vec3 zero_vec = {};
	const glm::vec3 identity_vec = { 1.0f, 1.0f, 1.0f };
	const glm::quat identity_quat = glm::quat_identity<float, glm::highp>();
	const glm::mat4 identity_mat = glm::mat4(1.0f);
	const glm::quat xRot90 = glm::angleAxis(PI / 2.0f, right_vec);
	const glm::quat yRot90 = glm::angleAxis(PI / 2.0f, up_vec);
	const glm::quat zRot90 = glm::angleAxis(PI / 2.0f, forward_vec);
	enum class Direction
	{
		RIGHT,
		UP,
		FORWARD
	};
};