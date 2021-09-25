#include "game_engine.hpp"

#include <iostream>
#include <thread>

int main() {
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