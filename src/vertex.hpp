#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <array>

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription get_binding_description()
	{
		// describes at which rate to load data from memory thoughout the vertices
		// it specifies the number of bytes between data entries and whether to 
		// move to the next data entry after each vertex or after each instance

		VkVertexInputBindingDescription binding_description{};
		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		// move to the next data entry after each vertex
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// move to the next data entry after each instance
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		return binding_description;
	}

	static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
	{
		// how to handle the vertex input

		// one attribute for pos and another for color
		std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, color);

		return attribute_descriptions;
	}
};