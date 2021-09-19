#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <vector>


struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	
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
		// binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE; // wow this was a source of alot of pain... 07/08/2021

		return binding_description;
	}

	static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions()
	{
		// how to handle the vertex input
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
		VkVertexInputAttributeDescription position_attr, color_attr, texCoord_attr;
		position_attr.binding = 0;
		position_attr.location = 0; // specify in shader
		position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		position_attr.offset = offsetof(Vertex, pos);

		color_attr.binding = 0;
		color_attr.location = 1; // specify in shader
		color_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		color_attr.offset = offsetof(Vertex, color);

		texCoord_attr.binding = 0;
		texCoord_attr.location = 2; // specify in shader
		texCoord_attr.format = VK_FORMAT_R32G32_SFLOAT;
		texCoord_attr.offset = offsetof(Vertex, texCoord);

		attribute_descriptions.push_back(position_attr);
		attribute_descriptions.push_back(color_attr);
		attribute_descriptions.push_back(texCoord_attr);

		return attribute_descriptions;
	}
};