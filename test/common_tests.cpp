#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>
#include <renderable/material_group.hpp>
#include <renderable/render_types.hpp>
#include <entity_component_system/material_system.hpp>
#include <utility.hpp>

#include <gtest/gtest.h>
#include <algorithm>

TEST(UtilityResources, test_mode_resolves_test_data_before_application_resources)
{
	EXPECT_EQ(
		Utility::get_model("simple_test_model.gltf"),
		Utility::get_top_level_path()/"test/data/simple_test_model.gltf");
	EXPECT_EQ(
		Utility::get_texture("texture.jpg"),
		Utility::get_top_level_path()/"resources/applications/default/textures/texture.jpg");
}

TEST(UtilityResources, resource_names_reject_absolute_paths_and_parent_traversal)
{
	EXPECT_THROW(Utility::get_model("/tmp/model.glb"), std::runtime_error);
	EXPECT_THROW(Utility::get_model("../model.glb"), std::runtime_error);
}

TEST(UtilityResources, collected_resources_are_filenames)
{
	const auto textures = Utility::get_all_textures();
	EXPECT_NE(std::ranges::find(textures, "texture.jpg"), textures.end());
	EXPECT_TRUE(std::ranges::all_of(textures, [](const std::string& filename)
	{
		return !std::filesystem::path(filename).is_absolute();
	}));
}


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
	const auto specular = make_texture(ETextureSemantic::SPECULAR);

	const TexturedMatGroup group({ specular, base });
	EXPECT_EQ(group.base_color_mat, base);
	EXPECT_FALSE(group.normal_mat);
	EXPECT_EQ(group.specular_mat, specular);
	EXPECT_EQ(group.get_materials(), (MatVec{ base, specular }));
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
