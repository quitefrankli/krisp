#include "test_helper.hpp"

#include <objects/object.hpp>
#include <renderable/renderable.hpp>
#include <serialization/serializer.hpp>
#include <serialization/serialization_helpers.hpp>

#include <gtest/gtest.h>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>

#include <ranges>


TEST(ObjectSerialization, excludes_transformation_and_parenting_state)
{
	Object object;
	Serializer serializer;
	object.serialize(serializer);
	const auto fields = Deserializer::parse(serializer.emit()).keys();

	EXPECT_EQ(std::ranges::find(fields, "world_transform"), fields.end());
	EXPECT_EQ(std::ranges::find(fields, "relative_transform"), fields.end());
	EXPECT_EQ(std::ranges::find(fields, "parent_id"), fields.end());
	EXPECT_EQ(std::ranges::find(fields, "aabb"), fields.end());
	EXPECT_EQ(std::ranges::find(fields, "renderables"), fields.end());
}

TEST(RenderableTransform, composes_gameplay_before_asset_local_transform)
{
	Renderable renderable;
	renderable.local_transform.set_pos({ 0.0f, 0.0f, 2.0f });
	const glm::mat4 gameplay = glm::rotate(
		Maths::identity_mat, Maths::deg2rad(90.0f), Maths::up_vec);

	const glm::vec3 world = glm::vec3(
		renderable.get_model_transform(gameplay) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	EXPECT_TRUE(glm_equal(world, glm::vec3(2.0f, 0.0f, 0.0f)));
}

TEST(RenderableFrameID, packs_without_cross_renderable_collision)
{
	const RenderableFrameID last_for_first_renderable(
		RenderableID(1), CSTS::UPPERBOUND_SWAPCHAIN_IMAGES - 1);
	const RenderableFrameID first_for_next_renderable(RenderableID(2), 0);
	EXPECT_EQ(
		last_for_first_renderable.get_underlying() + 1,
		first_for_next_renderable.get_underlying());
}
