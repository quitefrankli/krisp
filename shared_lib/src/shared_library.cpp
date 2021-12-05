#include "shared_library.hpp"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

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
void SharedLib::foo()
{
	glm::vec3 vec;
	// vec.x = 1.0f;
	std::cout << glm::to_string(vec) << std::endl;
}

extern "C" {
__declspec(dllexport) void __cdecl foo() {
	std::cout << "foo\n";
}
}