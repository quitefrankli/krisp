#pragma once

#include "pipeline_manager.hpp"
#include "pipeline_id.hpp"
#include "pipeline_modifiers.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "utility.hpp"
#include "pipelines.hpp"

#include <magic_enum.hpp>
#include <quill/LogMacros.h>

#include <cassert>
#include <iostream>


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	VkPipelineLayoutCreateInfo pipeline_layout_create_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	// TODO: this doesn't accouht for raytracing layout, however for now we are just manually keeping the two in
	// sync, eventually we need a way to specify a generic layout for both
	const auto descriptor_set_layouts = get_rsrc_mgr().get_rasterization_descriptor_set_layouts();
	pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts.size();
	pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_create_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_create_info.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(get_logical_device(), &pipeline_layout_create_info, nullptr, &generic_pipeline_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}
}

GraphicsEnginePipelineManager::~GraphicsEnginePipelineManager()
{
	vkDestroyPipelineLayout(get_logical_device(), generic_pipeline_layout, nullptr);
}

GraphicsEnginePipeline* GraphicsEnginePipelineManager::fetch_pipeline(PipelineID id)
{
	auto it = pipelines_by_id.find(id);
	if (it == pipelines_by_id.end())
	{
		return pipelines_by_id.emplace(id, create_pipeline(id)).first->second.get();
	}

	return it->second.get();
}

std::unique_ptr<GraphicsEnginePipeline> GraphicsEnginePipelineManager::create_pipeline(PipelineID id)
{
	std::unique_ptr<PipelineType> new_pipeline;

	switch (id.primary_pipeline_type)
	{
	case ERenderType::COLOR:
		new_pipeline = create_pipeline<ColorPipeline>(id);
		break;
	case ERenderType::STANDARD:
		new_pipeline = create_pipeline<TexturePipeline>(id);
		break;
	case ERenderType::CUBEMAP:
		new_pipeline = create_pipeline<CubemapPipeline>(id);
		break;
	case ERenderType::RAYTRACING:
		new_pipeline = create_pipeline<RaytracingPipeline>(id);
		break;
	case ERenderType::LIGHTWEIGHT_OFFSCREEN_PIPELINE:
		new_pipeline = create_pipeline<LightWeightOffscreenPipeline>(id);
		break;
	case ERenderType::SKINNED:
		new_pipeline = create_pipeline<SkinnedPipeline>(id);
		break;
	case ERenderType::QUAD:
		new_pipeline = create_pipeline<QuadPipeline>(id);
		break;
	case ERenderType::PARTICLE:
		new_pipeline = create_pipeline<ParticlePipeline>(id);
		break;
	default:
		throw std::runtime_error(
			std::string("GraphicsEnginePipelineManager::create_pipeline: invalid primary pipeline type: ") +
			std::string(magic_enum::enum_name(id.primary_pipeline_type)));
	}

	if (new_pipeline.get())
	{
		new_pipeline->initialise();
	}

	LOG_INFO(Utility::get_logger(), "created pipeline with id: {} {}", 
		magic_enum::enum_name(id.primary_pipeline_type),
		magic_enum::enum_name(id.pipeline_modifier));

	return new_pipeline;
}

template<typename PrimaryPipelineType>
std::unique_ptr<GraphicsEnginePipeline> GraphicsEnginePipelineManager::create_pipeline(PipelineID id)
{
	switch (id.pipeline_modifier)
	{
	case EPipelineModifier::NONE:
		return std::make_unique<PrimaryPipelineType>(get_graphics_engine());
	case EPipelineModifier::STENCIL:
		if constexpr (Stencileable<PrimaryPipelineType>)
		{
			return std::make_unique<StencilPipeline<PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	case EPipelineModifier::POST_STENCIL:
		if constexpr (Stencileable<PrimaryPipelineType>)
		{
			if constexpr (std::is_same_v<PrimaryPipelineType, ColorPipeline>)
			{
				return std::make_unique<PostStencilColorPipeline>(get_graphics_engine());
			} else if constexpr (std::is_same_v<PrimaryPipelineType, TexturePipeline>)
			{
				return std::make_unique<PostStencilTexturePipeline>(get_graphics_engine());
			} else if constexpr (std::is_same_v<PrimaryPipelineType, SkinnedPipeline>)
			{
				return std::make_unique<PostStencilSkinnedPipeline>(get_graphics_engine());
			} else {
				throw std::runtime_error("GraphicsEnginePipelineManager::create_pipeline: invalid primary pipeline type for POST_STENCIL");
			}
		}
		break;
	case EPipelineModifier::WIREFRAME:
		if constexpr (Wireframeable<PrimaryPipelineType>)
		{
			return std::make_unique<WireframePipeline<PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	case EPipelineModifier::SHADOW_MAP:
		if constexpr (ShadowMappable<PrimaryPipelineType>)
		{
			return std::make_unique<ShadowMapPipeline<PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	default:
		break;
	}
	
	LOG_WARNING(Utility::get_logger(), "create_pipeline failed with invalid pipeline id: {} {}",
		magic_enum::enum_name(id.primary_pipeline_type),
		magic_enum::enum_name(id.pipeline_modifier));
	return std::unique_ptr<GraphicsEnginePipeline>{nullptr};
}