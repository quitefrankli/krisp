#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>


namespace Maths
{
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

		const float s = std::sqrtf( (1+cosTheta)*2.0f );
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
		return glm::dot(ray.origin - plane.offset, ray.direction) < 0;
	}

	bool check_spherical_collision(const Ray& ray, const Sphere& sphere)
	{
		glm::vec3 projP = -glm::dot(ray.origin, ray.direction) * ray.direction + 
			ray.origin + glm::dot(ray.direction, sphere.origin) * ray.direction;
		return glm::distance(projP, sphere.origin) < sphere.radius;
	}

	const glm::vec3& Transform::get_pos()
	{
		if (is_old(0b1000))
		{
			position = transform[3];
			is_up_to_date |= 0b1000;				
		}
		return position; 
	}

	const glm::vec3& Transform::get_scale()
	{
		if (is_old(0b0100))
		{
			const auto get_vec3_len = [](const glm::vec4& vec4)
			{
				return std::sqrtf(vec4[0] * vec4[0] + vec4[1] * vec4[1] + vec4[2] * vec4[2]);
			};

			// https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati/417813
			scale[0] = get_vec3_len(transform[0]);
			scale[1] = get_vec3_len(transform[1]);
			scale[2] = get_vec3_len(transform[2]);
			
			is_up_to_date |= 0b0100;				
		}
		return scale;
	}

	const glm::quat& Transform::get_orient()
	{ 
		if (is_old(0b0010))
		{
			orientation = glm::normalize(glm::quat_cast(transform));
			is_up_to_date |= 0b0010;
		}
		return orientation;
	}

	const glm::mat4& Transform::get_mat4()
	{
		if (is_old(0b0001))
		{
			const auto tmp_orient = glm::normalize(get_orient());
			const auto tmp_scale = get_scale();
			const auto tmp_pos = get_pos();

			transform = glm::mat4_cast(tmp_orient) * glm::scale(tmp_scale);
			transform[3] = glm::vec4(tmp_pos, 1.0f);
			is_up_to_date |= 0b0001;
		}
		return transform;
	}

	void Transform::set_pos(const glm::vec3& new_pos)
	{
		position = new_pos;
		is_up_to_date |= 0b1000;
		is_up_to_date &= 0b1110;
	}

	void Transform::set_scale(const glm::vec3& new_scale)
	{
		scale = new_scale;
		is_up_to_date |= 0b0100;
		is_up_to_date &= 0b1110;
	}

	void Transform::set_orient(const glm::quat& new_orient)
	{
		orientation = new_orient;
		is_up_to_date |= 0b0010;
		is_up_to_date &= 0b1110;
	}

	void Transform::set_mat4(const glm::mat4& new_transform)
	{
		transform = new_transform;
		is_up_to_date = 0b0001;
	}	
}