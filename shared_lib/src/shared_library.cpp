#include "shared_library.hpp"

#include "objects/objects.hpp"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#define SHARED_LIB_EXPORT __attribute__((visibility("default")))

// Keep exported symbol names stable for dlsym.
extern "C" {
	SHARED_LIB_EXPORT void load_check()
	{
		std::cout << "shared_library loaded successfully!\n";
	}

	SHARED_LIB_EXPORT bool screen_to_world(glm::mat4& transform, glm::mat4 view, glm::mat4 proj, glm::vec2 screen)
	{
		proj[1][1] *= -1.0f;
		auto proj_view_mat = glm::inverse(proj * view);

		float scalar = 0.98f;
		auto tmp = proj_view_mat * glm::vec4(screen, scalar, 1.0f);

		auto p1 = proj_view_mat * glm::vec4(screen, 0.7f, 1.0f);
		auto p2 = proj_view_mat * glm::vec4(screen, 0.95f, 1.0f);
		p1 /= p1.w;
		p2 /= p2.w;
		auto direction = glm::vec3(p2 - p1);
		std::cout << glm::to_string(p1) << '\n';
		std::cout << glm::to_string(p2) << '\n';
		glm::vec3 forward(0.0f, 0.0f, -1.0f);
		auto quat = glm::rotation(forward, glm::normalize(direction));

		std::cout << scalar << ' ' << tmp.z << "\n";

		tmp /= tmp.w;
		// tmp.z = sqrtf(tmp.z);
		scalar -= 0.1f;

		transform = glm::mat4(1.0f);
		transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 5.0f));
		transform = glm::mat4_cast(quat) * transform;
		transform = glm::translate(glm::mat4(1.0f), glm::vec3(tmp)) * transform;
		return true;
	}

	SHARED_LIB_EXPORT bool simple_collision_detection(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& obj, float radius)
	{
		// assuming unit box, uses a sphere for efficiency
		glm::vec3 v1 = obj - rayOrigin;
		float parallelness = glm::dot(glm::normalize(v1), rayDir);
		float rad = 3.1415 / 2.0f * (1.0f - fabsf(parallelness));
		float distance = glm::length(v1) * sinf(rad);
		std::cout << glm::length(v1) << ' ' << sinf(rad) << ' ' << distance << ' ' << radius << '\n';

		return distance < radius;
	}

	SHARED_LIB_EXPORT void point_arrow(Arrow& v1)
	{
		std::cout << "SharedLibrary " << __FUNCTION__ << '\n';
		v1.point(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	SHARED_LIB_EXPORT void gen_vec(glm::vec3& vec)
	{
		// vec = {0.0f, 1.0f, 0.0f};
		vec = {0.0f, 0.0f, 1.0f};
	}

	SHARED_LIB_EXPORT SharedLibFuncPtrs shared_lib_func_ptrs {
		&load_check,
		&point_arrow,
		&gen_vec
	};
}
