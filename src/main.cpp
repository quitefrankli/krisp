#include "graphics_engine.hpp"

int main() {
    GraphicsEngine graphics_engine;

    try {
        graphics_engine.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
	} catch (...) {
		std::cout << "Exception Thrown!\n";
	}

    return EXIT_SUCCESS;
}