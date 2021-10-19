#include "game_engine.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <thread>


quill::Logger* logger;

int main() {
	quill::enable_console_colours();
	quill::start();

	auto file_handler = quill::file_handler("log.log", "a");
	logger = quill::create_logger("MAIN", file_handler);

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