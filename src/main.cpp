#include "graphics_engine.hpp"
#include "vertex.hpp"

#include <glm/glm.hpp>

int main() {
    GraphicsEngine graphics_engine;
	std::vector<Vertex> vertices
	{
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};
	graphics_engine.set_vertices(vertices);
	graphics_engine.binding_description = Vertex::get_binding_description();
	auto attribute_descriptions = Vertex::get_attribute_descriptions();
	graphics_engine.attribute_descriptions = std::vector<VkVertexInputAttributeDescription>(
		attribute_descriptions.begin(), attribute_descriptions.end()
	);

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