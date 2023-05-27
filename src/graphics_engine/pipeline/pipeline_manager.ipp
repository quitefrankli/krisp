#pragma once

#include "pipeline_manager.hpp"
#include "pipeline_id.hpp"
#include "pipeline_modifiers.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "utility.hpp"

#include <magic_enum.hpp>
#include <quill/Quill.h>

#include <cassert>
#include <iostream>


template<typename GraphicsEngineT>
GraphicsEnginePipelineManager<GraphicsEngineT>::GraphicsEnginePipelineManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
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

template<typename GraphicsEngineT>
GraphicsEnginePipelineManager<GraphicsEngineT>::~GraphicsEnginePipelineManager()
{
	vkDestroyPipelineLayout(get_logical_device(), generic_pipeline_layout, nullptr);
}

template<typename GraphicsEngineT>
GraphicsEnginePipeline<GraphicsEngineT>* GraphicsEnginePipelineManager<GraphicsEngineT>::fetch_pipeline(PipelineID id)
{
	auto it = pipelines_by_id.find(id);
	if (it == pipelines_by_id.end())
	{
		return pipelines_by_id.emplace(id, create_pipeline(id)).first->second.get();
	}

	return it->second.get();
}

template<typename GraphicsEngineT>
std::unique_ptr<GraphicsEnginePipeline<GraphicsEngineT>> GraphicsEnginePipelineManager<GraphicsEngineT>::create_pipeline(PipelineID id)
{
	std::unique_ptr<PipelineType> new_pipeline;

	switch (id.primary_pipeline_type)
	{
	case EPipelineType::COLOR:
		new_pipeline = create_pipeline<ColorPipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::STANDARD:
		new_pipeline = create_pipeline<TexturePipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::CUBEMAP:
		new_pipeline = create_pipeline<CubemapPipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::RAYTRACING:
		new_pipeline = create_pipeline<RaytracingPipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::LIGHTWEIGHT_OFFSCREEN_PIPELINE:
		new_pipeline = create_pipeline<LightWeightOffscreenPipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::SKINNED:
		new_pipeline = create_pipeline<SkinnedPipeline<GraphicsEngineT>>(id);
		break;
	case EPipelineType::QUAD:
		new_pipeline = create_pipeline<QuadPipeline<GraphicsEngineT>>(id);
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

	LOG_INFO(Utility::get().get_logger(), "created pipeline with id: {} {}", 
		magic_enum::enum_name(id.primary_pipeline_type),
		magic_enum::enum_name(id.pipeline_modifier));

	return new_pipeline;
}

template<typename GraphicsEngineT>
template<typename PrimaryPipelineType>
std::unique_ptr<GraphicsEnginePipeline<GraphicsEngineT>> GraphicsEnginePipelineManager<GraphicsEngineT>::create_pipeline(PipelineID id)
{
	switch (id.pipeline_modifier)
	{
	case EPipelineModifier::NONE:
		return std::make_unique<PrimaryPipelineType>(get_graphics_engine());
	case EPipelineModifier::STENCIL:
		if constexpr (Stencileable<PrimaryPipelineType>)
		{
			return std::make_unique<StencilPipeline<GraphicsEngineT, PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	case EPipelineModifier::WIREFRAME:
		if constexpr (Wireframeable<PrimaryPipelineType>)
		{
			return std::make_unique<WireframePipeline<GraphicsEngineT, PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	case EPipelineModifier::SHADOW_MAP:
		if constexpr (ShadowMappable<PrimaryPipelineType>)
		{
			return std::make_unique<ShadowMapPipeline<GraphicsEngineT, PrimaryPipelineType>>(get_graphics_engine());
		}
		break;
	default:
		break;
	}
	
	LOG_WARNING(Utility::get().get_logger(), "create_pipeline failed with invalid pipeline id: {} {}",
		magic_enum::enum_name(id.primary_pipeline_type),
		magic_enum::enum_name(id.pipeline_modifier));
	return std::unique_ptr<GraphicsEnginePipeline<GraphicsEngineT>>{nullptr};
}