#include "game_engine.hpp"
#include "maths.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <thread>
#include <iomanip>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
// #include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>


quill::Logger* logger;
std::string RELATIVE_BINARY_PATH;

int main(int argc, char* argv[]) {
	RELATIVE_BINARY_PATH = argv[0];
#ifdef NDEBUG
	std::cout << "Release Mode\n";
#else
	std::cout << "Debug Mode\n";
#endif

	auto file_handler = quill::file_handler("log.log", "a");
	logger = quill::create_logger("MAIN", file_handler);
	file_handler->set_pattern(
		QUILL_STRING("%(ascii_time): %(message)"),
		"%D %H:%M:%S.%Qus",
		quill::Timezone::LocalTime
	);
	// quill::start(); // this will consume CPU cycles

	// seems like glfw window must be on main thread otherwise it wont work, therefore engine should always be on its own thread
	GameEngine engine;

    try {
		engine.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
	} catch (...) {
		std::cout << "Exception Thrown!\n";
	}

    return EXIT_SUCCESS;
}