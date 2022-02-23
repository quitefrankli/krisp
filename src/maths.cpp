#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include <iostream>


namespace Maths
{
	// taken from http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/
	glm::quat RotationBetweenVectors(const glm::vec3& start, const glm::vec3& end)
	{
		float cosTheta = glm::dot(start, end);
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
		} else if (cosTheta > 1 - 0.001f)
		{
			return glm::quat();
		}

		rotationAxis = glm::cross(start, end);

		float s = std::sqrtf( (1+cosTheta)*2.0f );
		float invs = 1.0f / s;

		return glm::quat(
			s * 0.5f, 
			rotationAxis.x * invs,
			rotationAxis.y * invs,
			rotationAxis.z * invs
		);
	}

	glm::quat Vec2Rot(const glm::vec3& vec)
	{
		return RotationBetweenVectors(forward_vec, vec);
	}
}