#include "graphics_engine/render_draw_list.hpp"

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
	const glm::vec3 local_offset = {})
{
	auto mesh = MeshSystem::add(MeshFactory::cube());
	auto material = MaterialSystem::add(std::make_unique<ColorMaterial>());
	return {
		.pipeline_render_type = render_type,
		.alpha_mode = alpha_mode,
		.casts_shadow = casts_shadow,
		.render_on_top = render_on_top,
		.local_transform = glm::translate(glm::mat4(1.0f), local_offset),
		.mesh_owner = std::move(mesh),
		.material_owners = { std::move(material) },
	};
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
}

TEST(RenderDrawList, state_sort_key_is_deterministic)
{
	const auto first = make_renderable(
		ERenderType::COLOR, EAlphaMode::OPAQUE, true, false);
	const auto second = make_renderable(
		ERenderType::STANDARD, EAlphaMode::MASK, true, false);

	std::vector keys{
		make_render_sort_key(ObjectID(9), 1, second),
		make_render_sort_key(ObjectID(4), 0, first),
		make_render_sort_key(ObjectID(3), 0, first),
	};
	std::sort(keys.begin(), keys.end());

	EXPECT_EQ(keys[0].render_type, ERenderType::STANDARD);
	EXPECT_EQ(keys[1].object_id, ObjectID(3));
	EXPECT_EQ(keys[2].object_id, ObjectID(4));
}

TEST(RenderDrawList, blended_distance_uses_renderable_local_transform)
{
	const auto renderable = make_renderable(
		ERenderType::STANDARD,
		EAlphaMode::BLEND,
		true,
		false,
		glm::vec3(0.0f, 0.0f, 5.0f));
	const glm::mat4 object_transform =
		glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));

	EXPECT_FLOAT_EQ(
		renderable_distance_squared({}, object_transform, renderable),
		49.0f);
}
