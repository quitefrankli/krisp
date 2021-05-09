#include "graphics_engine.hpp"

#include <vector>
#include <fstream>

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
	
	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

static VkShaderModule create_shader_module(const std::vector<char>& code, VkDevice& logical_device)
{
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(logical_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shader_module;
}

void GraphicsEngine::create_graphics_pipeline() {
    auto vertShaderCode = readFile("vertex_shader.spv");
    auto fragShaderCode = readFile("fragment_shader.spv");

	VkShaderModule vertex_shader = create_shader_module(vertShaderCode, logical_device);
	VkShaderModule fragment_shader = create_shader_module(fragShaderCode, logical_device);

	VkPipelineShaderStageCreateInfo vertex_shader_create_info{};
	vertex_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT; // tells vulkan which pipeline stage the shader is going to be used
	vertex_shader_create_info.module = vertex_shader;
	vertex_shader_create_info.pName = "main"; // function to invoke aka entrypoint
	// vertex_shader_create_info.pSpecializationInfo = // allows for specification of shader constants

	VkPipelineShaderStageCreateInfo fragment_shader_create_info{};
	fragment_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_create_info.module = fragment_shader;
	fragment_shader_create_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_create_info, fragment_shader_create_info };

	vkDestroyShaderModule(logical_device, vertex_shader, nullptr);
	vkDestroyShaderModule(logical_device, fragment_shader, nullptr);
}