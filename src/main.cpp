#include "game_engine.hpp"
#include "maths.hpp"

#include <quill/Quill.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
// #include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <iostream>
#include <thread>
#include <iomanip>
// #include <format> // unfortunately only availble in C++20 which MSVC does not have yet
#include <filesystem>


quill::Logger* logger;
std::filesystem::path BINARY_DIRECTORY;
std::filesystem::path WORKING_DIRECTORY; // can be thought as the build directory, but should be the most upper level directory

int main(int argc, char* argv[]) {
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

	// parse command line arguments
	for (int i = 0; i < argc; i++)
	{
		std::string_view arg(argv[i]);
		if (i == 0)
		{
			BINARY_DIRECTORY = arg;
			BINARY_DIRECTORY = BINARY_DIRECTORY.parent_path();
			WORKING_DIRECTORY = BINARY_DIRECTORY.parent_path();
			fmt::print("BINARY_DIRECTORY:={}, WORKING_DIRECTORY:={}\n", BINARY_DIRECTORY.string(), WORKING_DIRECTORY.string());
		} else if (arg == "--logging")
		{
			quill::start(); // this will consume CPU cycles
		} else {
			fmt::print("ERROR: invalid cmdline argument: '{}'\n", arg);
			return EXIT_FAILURE;
		}
	}

    try {
		bool restart_signal = false;
		do {
			// seems like glfw window must be on main thread otherwise it wont work, 
			// therefore engine should always be on its own thread
			restart_signal = false;
			GameEngine engine([&restart_signal](){restart_signal=true;});
			engine.run();
		} while (restart_signal);
    } catch (const std::exception& e) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: {}\n", e.what());
        return EXIT_FAILURE;
	} catch (...) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: UNKNOWN\n");
        return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}