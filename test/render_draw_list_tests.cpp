#include "graphics_engine/render_draw_list.hpp"
#include "graphics_engine/pipeline/pipeline_id.hpp"

#include "renderable/material.hpp"
#include "renderable/mesh_factory.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>


namespace
{
RenderableDefinition make_renderable(
	const ERenderType render_type,
	const EAlphaMode alpha_mode,
	const bool casts_shadow,
	const bool render_on_top,
	const EShadingMode shading_mode = EShadingMode::LIT,
	const bool double_sided = false)
{
	MeshSystem meshes;
	MaterialSystem materials;
	auto mesh = meshes.add(MeshFactory::cube());
	auto material = materials.add(std::make_unique<PbrMaterial>(
		glm::vec4(1.0f), 1.0f, 1.0f,
		PbrMaterial::TextureSlots{}, 1.0f,
		PbrMaterial::Properties{
			.alpha_mode = alpha_mode,
			.double_sided = double_sided,
		}));
	return {
		.pipeline_render_type = render_type,
		.shading_mode = shading_mode,
		.casts_shadow = casts_shadow,
		.render_on_top = render_on_top,
		.mesh_owner = std::move(mesh),
		.material_owners = { std::move(material) },
	};
}

TEST(RenderDrawList, derives_alpha_and_culling_from_pbr_material)
{
	const auto renderable = make_renderable(
		ERenderType::STANDARD, EAlphaMode::MASK, true, false,
		EShadingMode::LIT, true);

	EXPECT_EQ(renderable_alpha_mode(renderable), EAlphaMode::MASK);
	EXPECT_FLOAT_EQ(renderable_alpha_cutoff(renderable), 0.5f);
	EXPECT_TRUE(renderable_double_sided(renderable));
	const auto key = make_render_sort_key(RenderableID(7), renderable);
	EXPECT_EQ(key.alpha_mode, EAlphaMode::MASK);
	EXPECT_TRUE(key.double_sided);
}

}


TEST(RenderDrawList, classifies_each_renderable_independently)
{
	const auto regular = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false);
	const auto overlay = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, false, true);
	const auto blended = make_renderable(
		ERenderType::STANDARD, EAlphaMode::BLEND, true, false);

	EXPECT_EQ(classify_renderable(regular), RenderableDrawClass::OPAQUE);
	EXPECT_EQ(classify_renderable(overlay), RenderableDrawClass::OVERLAY_OPAQUE);
	EXPECT_EQ(classify_renderable(blended), RenderableDrawClass::BLENDED);
	EXPECT_TRUE(renderable_casts_shadow(regular));
	EXPECT_FALSE(renderable_casts_shadow(overlay));
	EXPECT_FALSE(renderable_casts_shadow(blended));
	const auto unlit = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false, EShadingMode::UNLIT);
	EXPECT_FALSE(renderable_casts_shadow(unlit));
}

TEST(RenderDrawList, non_pbr_draw_types_use_safe_material_policy_defaults)
{
	const RenderableDefinition non_pbr{
		.pipeline_render_type = ERenderType::CUBEMAP,
	};

	EXPECT_EQ(renderable_alpha_mode(non_pbr), EAlphaMode::OPAQUE);
	EXPECT_FLOAT_EQ(renderable_alpha_cutoff(non_pbr), 0.5f);
	EXPECT_FALSE(renderable_double_sided(non_pbr));
	EXPECT_EQ(classify_renderable(non_pbr), RenderableDrawClass::OPAQUE);
}

TEST(RenderDrawList, state_sort_key_is_deterministic)
{
	const auto first = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false);
	const auto second = make_renderable(
		ERenderType::STANDARD, EAlphaMode::MASK, true, false);

	std::vector keys{
		make_render_sort_key(RenderableID(9), second),
		make_render_sort_key(RenderableID(4), first),
		make_render_sort_key(RenderableID(3), first),
	};
	std::sort(keys.begin(), keys.end());

	EXPECT_EQ(keys[0].render_type, ERenderType::STANDARD);
	EXPECT_EQ(keys[1].renderable_id, RenderableID(3));
	EXPECT_EQ(keys[2].renderable_id, RenderableID(4));
}

TEST(RenderDrawList, shading_is_an_independent_pipeline_and_sort_dimension)
{
	const PipelineID lit_selected{
		.primary_pipeline_type = ERenderType::COLOR,
		.pipeline_modifier = EPipelineModifier::POST_STENCIL,
		.shading_mode = EShadingMode::LIT,
	};
	const PipelineID unlit_selected{
		.primary_pipeline_type = ERenderType::COLOR,
		.pipeline_modifier = EPipelineModifier::POST_STENCIL,
		.shading_mode = EShadingMode::UNLIT,
	};
	EXPECT_NE(lit_selected, unlit_selected);
	EXPECT_NE(std::hash<PipelineID>{}(lit_selected), std::hash<PipelineID>{}(unlit_selected));

	const auto lit = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false);
	const auto unlit = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false, EShadingMode::UNLIT);
	EXPECT_EQ(make_render_sort_key(RenderableID(1), lit).shading_mode, EShadingMode::LIT);
	EXPECT_EQ(make_render_sort_key(RenderableID(2), unlit).shading_mode, EShadingMode::UNLIT);
}

TEST(RenderDrawList, double_sided_is_an_independent_pipeline_dimension)
{
	const PipelineID single_sided{
		.primary_pipeline_type = ERenderType::STANDARD,
		.double_sided = false,
	};
	const PipelineID double_sided{
		.primary_pipeline_type = ERenderType::STANDARD,
		.double_sided = true,
	};

	EXPECT_NE(single_sided, double_sided);
	EXPECT_NE(std::hash<PipelineID>{}(single_sided), std::hash<PipelineID>{}(double_sided));
}

TEST(RenderDrawList, blended_distance_uses_composed_model_transform)
{
	const auto renderable = make_renderable(
		ERenderType::STANDARD,
		EAlphaMode::BLEND,
		true,
		false);
	const glm::mat4 model_transform =
		glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));

	EXPECT_FLOAT_EQ(
		renderable_distance_squared({}, model_transform),
		4.0f);
}
