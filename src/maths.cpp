#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>
#include <random>


namespace Maths
{
	static std::mt19937 randGen(std::random_device{}());

	template<typename T>
	T random_uniform(T min, T max)
	{
		if (min > max)
		{
			return std::uniform_real_distribution<T>(max, min)(randGen);
		}

		return std::uniform_real_distribution<T>(min, max)(randGen);
	}

	template float random_uniform<float>(float min, float max);

	template<>
	int random_uniform<int>(int min, int max)
	{
		if (min > max)
		{
			return std::uniform_int_distribution<int>(max, min)(randGen);
		}

		return std::uniform_int_distribution<int>(min, max)(randGen);
	}

	template<>
	glm::vec3 random_uniform<glm::vec3>(glm::vec3 min, glm::vec3 max)
	{
		return glm::vec3{
			random_uniform(min.x, max.x),
			random_uniform(min.y, max.y),
			random_uniform(min.z, max.z)
		};
	}

	template<typename T>
	T random_normal(T mean, T stdDev)
	{
		if (stdDev <= 0)
		{
			return mean;
		}
		
		return std::normal_distribution<T>(mean, stdDev)(randGen);
	}

	template float random_normal<float>(float mean, float stdDev);

	template<typename T>
	T lerp(T a, T b, float t)
	{
		return a + t * (b - a);
	}

	template glm::vec4 lerp<glm::vec4>(glm::vec4 a, glm::vec4 b, float t);

	// taken from http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end)
	{
		const float cosTheta = glm::dot(start, end);
		glm::vec3 rotationAxis;

		if (cosTheta < -1 + 0.001f){
			// special case when vectors in opposite directions:
			// there is no "ideal" rotation axis
			// So guess one; any will do as long as it's perpendicular to start
			rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
			if (glm::length(rotationAxis) < 0.01 ) // bad luck, they were parallel, try again!
				rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);

			rotationAxis = normalize(rotationAxis);
			return glm::angleAxis(glm::radians(180.0f), rotationAxis);
		} else if (cosTheta > 1 - 0.001f) // same direction, so no rotation
		{
			return glm::quat_identity<float, glm::highp>();
		}

		rotationAxis = glm::cross(start, end);

		const float s = Maths::sqrtf( (1+cosTheta)*2.0f );
		const float invs = 1.0f / s;

		return glm::quat(
			s * 0.5f, 
			rotationAxis.x * invs,
			rotationAxis.y * invs,
			rotationAxis.z * invs
		);
	}

	// inspired by
	// https://stackoverflow.com/questions/5188561/signed-angle-between-two-3d-vectors-with-same-origin-within-the-same-plane
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end, const glm::vec3& axis)
	{
		// essentially this takes advantage of the fact that 
		// (Va x Vb) . axis == 1 (if +ve angle) and -1 (if -ve angle)
		// atan2 is necessary otherwise the angle is ambiguous
		const float numerator = glm::dot(glm::cross(start, end), axis);
		const float denominator = glm::dot(start, end);
		const float angle = atan2f(numerator, denominator);
		return glm::angleAxis(angle, axis);
	}

	glm::quat Vec2Rot(const glm::vec3& vec)
	{
		return RotationBetweenVectors(forward_vec, vec);
	}

	glm::vec3 ray_plane_intersection(const Ray& ray, const Plane& plane)
	{
		// inspired by 
		// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection
		// using the equations
		// P = some point on plane
		// L = some scalar length
		// Pn, Po = plane normal, plane offset
		// Ro, Rn = ray offset, ray normal

		// P = LRn + Ro
		// (P - Po) * Pn = 0

		// solving for L and then back substituting eqn 1 gives P

		const float L = glm::dot(plane.offset - ray.origin, plane.normal) / glm::dot(ray.direction, plane.normal);

		return L * ray.direction + ray.origin;
	}

	bool check_ray_plane_intersection(const Ray& ray, const Plane& plane)
	{
		if (is_vec3_equal(ray.origin, plane.offset))
		{
			return true;
		}
		return glm::dot(ray.origin - plane.offset, ray.direction) < 0;
	}

	bool check_spherical_collision(const Ray& ray, const Sphere& sphere)
	{
		glm::vec3 projP = -glm::dot(ray.origin, ray.direction) * ray.direction + 
			ray.origin + glm::dot(ray.direction, sphere.origin) * ray.direction;
		return glm::distance(projP, sphere.origin) < sphere.radius;
	}

	bool check_ray_rod_collision(const Ray& ray, const glm::vec3& rod_start, const glm::vec3& rod_end, const float radius)
	{
		const glm::vec3 axis = glm::normalize(rod_end - rod_start);
		const float length = glm::distance(rod_start, rod_end);
		const Maths::Sphere collision_sphere(
			(rod_start + rod_end) * 0.5f,
			length * 0.5f
		);
		if (!Maths::check_spherical_collision(ray, collision_sphere))
		{
			return false;
		}

		// the cross product of Rd and Ad gives the normal that will contain the
		// segment with the shortest distance assuming there is a collision
		const glm::vec3 normal = glm::normalize(glm::cross(ray.direction, axis));
		// projecting Ro and Ao onto said normal tells us given two infinite rays
		// the shortest distance between any two points on the two.
		float dist = Maths::absf(glm::dot(rod_start, normal) - glm::dot(ray.origin, normal));
		// a cylinder is just a ray with a radius, so if the shortest possible distance
		// is greater than the radius of the cylinder then there is no intersection

		return dist < radius;
	}

	bool check_ray_rod_collision(const Ray& ray,
                                 const glm::vec3& rod_start,
                                 const glm::vec3& rod_end,
								 const float radius,
                                 glm::vec3& out_intersection)
	{
		const glm::vec3 axis = glm::normalize(rod_end - rod_start);
		const float length = glm::distance(rod_start, rod_end);
		const Maths::Sphere collision_sphere(
			(rod_start + rod_end) * 0.5f,
			length * 0.5f
		);
		if (!Maths::check_spherical_collision(ray, collision_sphere))
		{
			return false;
		}

		// the cross product of Rd and Ad gives the normal that will contain the
		// segment with the shortest distance assuming there is a collision
		const glm::vec3 normal = glm::normalize(glm::cross(ray.direction, axis));
		// projecting Ro and Ao onto said normal tells us given two infinite rays
		// the shortest distance between any two points on the two.
		float dist = Maths::absf(glm::dot(rod_start, normal) - glm::dot(ray.origin, normal));
		// a cylinder is just a ray with a radius, so if the shortest possible distance
		// is greater than the radius of the cylinder then there is no intersection
		if (dist > radius)
		{
			return false;
		}

		// TODO: clean this up

		// Y = vector perpendicular to normal and arrow direction
		const glm::vec3 Y = glm::normalize(glm::cross(axis, normal));
		const float Ad_Y = glm::dot(axis, Y);				// Ad . Y
		const float Ad_Ad = glm::dot(axis, axis);			// Ad . Ad
		const glm::vec3 Ao_Ro = rod_start - ray.origin;	// Ao - Ro

		const float numerator = Ad_Y * glm::dot(Ao_Ro, axis) + Ad_Ad * glm::dot(-Ao_Ro, Y);
		const float denominator = Ad_Y * glm::dot(ray.direction, axis) - Ad_Ad * glm::dot(ray.direction, Y);
		const float t = numerator / denominator; // P = Ro + tRd
		out_intersection = ray.origin + t * ray.direction;

		// idk why the above intersection is wrong although I have theory,
		// anyways the below code is a quick hack to get it to work

		const float rr = radius * radius;
		const float xx = dist * dist;
		if (rr < xx)
		{
			std::cerr << "Arrow::check_collision(Ray, Intersection): no quadratic solution!\n";
			return false;
		}
		
		const float y = Maths::sqrtf(rr - xx);
		const glm::vec3 pA = out_intersection - y*Y;
		const glm::vec3 pB = out_intersection + y*Y;

		out_intersection = glm::dot(pA, ray.direction) < glm::dot(pB, ray.direction) ? pA : pB;

		return true;
	}

	const glm::vec3& Transform::get_pos() const
	{
		if (is_old(0b1000))
		{
			assert(!is_old(0b0001));
			position = transform[3];
			set_not_old(0b1000);
		}
		return position; 
	}

	const glm::vec3& Transform::get_scale() const
	{
		if (is_old(0b0100))
		{
			assert(!is_old(0b0001));
			// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/417813
			const auto get_vec3_len = [](const glm::vec4& vec4)
			{
				return Maths::sqrtf(vec4[0] * vec4[0] + vec4[1] * vec4[1] + vec4[2] * vec4[2]);
			};

			scale[0] = get_vec3_len(transform[0]);
			scale[1] = get_vec3_len(transform[1]);
			scale[2] = get_vec3_len(transform[2]);
			
			set_not_old(0b0100);

		}
		return scale;
	}

	const glm::quat& Transform::get_orient() const
	{ 
		if (is_old(0b0010))
		{
			assert(!is_old(0b0001));
			// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/417813
			const auto scale = get_scale();
			const glm::mat3 rotMtx(
				glm::vec3(transform[0]) / scale[0],
				glm::vec3(transform[1]) / scale[1],
				glm::vec3(transform[2]) / scale[2]);
    		orientation = glm::quat_cast(rotMtx);
			// orientation = glm::quat_cast(transform);
			set_not_old(0b0010);
		}
		return orientation;
	}

	const glm::mat4& Transform::get_mat4() const
	{
		if (is_old(0b0001))
		{
			assert(is_up_to_date_flags == 0b1110);
			const auto tmp_orient = glm::normalize(get_orient());
			const auto tmp_scale = get_scale();
			const auto tmp_pos = get_pos();

			transform =  glm::scale(glm::mat4_cast(tmp_orient), tmp_scale);
			transform[3] = glm::vec4(tmp_pos, 1.0f);
			set_not_old(0b0001);
		}
		return transform;
	}

	void Transform::set_pos(const glm::vec3& new_pos)
	{
		position = new_pos;
		is_up_to_date_flags |= 0b1000;
		is_up_to_date_flags &= 0b1110;
	}

	void Transform::set_scale(const glm::vec3& new_scale)
	{
		scale = new_scale;
		is_up_to_date_flags |= 0b0100;
		is_up_to_date_flags &= 0b1110;
	}

	void Transform::set_orient(const glm::quat& new_orient)
	{
		orientation = new_orient;
		is_up_to_date_flags |= 0b0010;
		is_up_to_date_flags &= 0b1110;
	}

	void Transform::set_mat4(const glm::mat4& new_transform)
	{
		transform = new_transform;
		is_up_to_date_flags = 0b0001;
		get_pos();
		get_orient();
		get_scale();
	}	

	std::optional<glm::vec3> ray_sphere_collision(const Sphere& sphere, const Ray& ray)
	{
		// special case when ray origin is inside sphere
		if (glm::distance2(sphere.origin, ray.origin) < sphere.radius * sphere.radius)
		{
			return ray.origin;
		}

		const float x_t = glm::dot(sphere.origin - ray.origin, ray.direction);
		// x is the closest point on the ray to sphere's center
		const glm::vec3 x = ray.origin + ray.direction * x_t;
		const float h = glm::distance(sphere.origin, x);
		// l is the distance between x and the surface of the sphere
		const float l = Maths::sqrtf(std::pow(sphere.radius, 2.0f) - std::pow(h, 2.0f));
		const float t = x_t - l;
		if (t < 0)
		{
			return std::nullopt;
		}
		
		return ray.origin + ray.direction * t;
	}
}