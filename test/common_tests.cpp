#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>
#include <renderable/render_types.hpp>

#include <gtest/gtest.h>


TEST(Basics, MaterialMoveCtor)
{
	ColorMaterial material;
	material.data.shininess = 2.5f;

	std::unique_ptr<Material> ptr = std::make_unique<ColorMaterial>(std::move(material));
	auto& material2 = static_cast<ColorMaterial&>(*ptr);

	ASSERT_EQ(material2.data.shininess, 2.5f);
}

TEST(Basics, SkinnedRenderTypeClassification)
{
	EXPECT_TRUE(is_skinned_render_type(ERenderType::SKINNED));
	EXPECT_TRUE(is_skinned_render_type(ERenderType::SKINNED_COLOR));
	EXPECT_FALSE(is_skinned_render_type(ERenderType::COLOR));
	EXPECT_FALSE(is_skinned_render_type(ERenderType::STANDARD));
}
