#pragma once

#include "materials.hpp"

#include <vulkan/vulkan.h>


// needs to be synced in shader
struct MaterialData
{
	glm::vec3 color;
	float roughness;
	float specular;
};

struct GraphicsMaterial
{
public:
	GraphicsMaterial(const Material& material)
	{
		data.color = material.color;
		data.roughness = material.roughness;
		data.specular = material.specular;
	}

private:
	VkSampler sampler;

	MaterialData data;
};