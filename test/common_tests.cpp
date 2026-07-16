#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>
#include <renderable/material_group.hpp>
#include <renderable/render_types.hpp>
#include <entity_component_system/material_system.hpp>

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

TEST(Basics, TexturedMaterialGroupResolvesOptionalMapsBySemantic)
{
	const auto make_texture = [](const ETextureSemantic semantic)
	{
		auto texture = std::make_unique<TextureMaterial>();
		texture->semantic = semantic;
		return MaterialSystem::add(std::move(texture));
	};
	const auto base = make_texture(ETextureSemantic::BASE_COLOR);
	const auto strength = make_texture(ETextureSemantic::SPECULAR_STRENGTH);
	const auto color = make_texture(ETextureSemantic::SPECULAR_COLOR);

	const TexturedMatGroup group({ color, base, strength });
	EXPECT_EQ(group.base_color_mat, base);
	EXPECT_FALSE(group.normal_mat);
	EXPECT_EQ(group.specular_strength_mat, strength);
	EXPECT_EQ(group.specular_color_mat, color);
	EXPECT_EQ(group.get_materials(), (MatVec{ base, strength, color }));
}

TEST(Basics, TexturedMaterialGroupRejectsDuplicateSemantics)
{
	auto first = std::make_unique<TextureMaterial>();
	first->semantic = ETextureSemantic::BASE_COLOR;
	auto second = std::make_unique<TextureMaterial>();
	second->semantic = ETextureSemantic::BASE_COLOR;
	const auto first_id = MaterialSystem::add(std::move(first));
	const auto second_id = MaterialSystem::add(std::move(second));

	EXPECT_THROW(TexturedMatGroup({ first_id, second_id }), std::runtime_error);
}
