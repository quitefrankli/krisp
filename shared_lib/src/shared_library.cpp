#include "shared_library.hpp"

#include "objects/objects.hpp"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <windows.h>

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved )  // reserved
{
    // Perform actions based on the reason for calling.
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
         // Initialize once for each new process.
         // Return FALSE to fail DLL load.
		 std::cout << "process attach\n";
            break;

        case DLL_THREAD_ATTACH:
         // Do thread-specific initialization.
		 std::cout << "thread attach\n";
            break;

        case DLL_THREAD_DETACH:
         // Do thread-specific cleanup.
		 std::cout << "thread detach\n";
            break;

        case DLL_PROCESS_DETACH:
		 std::cout << "process detach\n";
         // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}

// compilers will by default mangle c++ objects i.e. Foo -> "?Foo@@YAHH@Z"
// extern C tells the compiler not to do that, that way when we GetProcAddress we don't need to use a mangled name
extern "C" {
	__declspec(dllexport) void __cdecl load_check()
	{
		std::cout << "shared_library loaded successfully!\n";
	}

	__declspec(dllexport) bool __cdecl screen_to_world(glm::mat4& transform, glm::mat4 view, glm::mat4 proj, glm::vec2 screen)
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

	__declspec(dllexport) bool __cdecl simple_collision_detection(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& obj, float radius)
	{
		// assuming unit box, uses a sphere for efficiency
		glm::vec3 v1 = obj - rayOrigin;
		float parallelness = glm::dot(glm::normalize(v1), rayDir);
		float rad = 3.1415 / 2.0f * (1.0f - fabsf(parallelness));
		float distance = glm::length(v1) * sinf(rad);
		std::cout << glm::length(v1) << ' ' << sinf(rad) << ' ' << distance << ' ' << radius << '\n';

		return distance < radius;
	}

	__declspec(dllexport) void __cdecl linear_alg(Arrow& v1)
	{
		std::cout << "SharedLibrary " << __FUNCTION__ << '\n';
		v1.point(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	__declspec(dllexport)SharedLibFuncPtrs shared_lib_func_ptrs {
		&load_check
	};
}