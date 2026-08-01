#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>
#include <renderable/material_group.hpp>
#include <renderable/render_types.hpp>
#include <entity_component_system/material_system.hpp>
#include <analytics.hpp>
#include <utility.hpp>

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>


static_assert(std::is_same_v<
	decltype(std::declval<MaterialSystem&>().get(std::declval<MaterialID>())),
	const Material&>);

TEST(AnalyticsStatistics, tracks_average_standard_deviation_and_range)
{
	Analytics::Statistics statistics;
	for (const double sample : { 1.0, 2.0, 3.0, 4.0 })
		statistics.add(sample);

	EXPECT_DOUBLE_EQ(statistics.average(), 2.5);
	EXPECT_NEAR(statistics.standard_deviation(), std::sqrt(1.25), 0.000001);
	EXPECT_DOUBLE_EQ(statistics.minimum(), 1.0);
	EXPECT_DOUBLE_EQ(statistics.maximum(), 4.0);
}

TEST(UtilityLoopSleeper, elapsed_work_counts_towards_loop_period)
{
	Utility::LoopSleeper sleeper(std::chrono::milliseconds(40));
	std::this_thread::sleep_for(std::chrono::milliseconds(45));

	const auto before_sleep = std::chrono::steady_clock::now();
	sleeper();
	const auto sleep_time = std::chrono::steady_clock::now() - before_sleep;

	EXPECT_LT(sleep_time, std::chrono::milliseconds(20));
}

TEST(UtilityResources, test_mode_resolves_test_data_before_project_resources)
{
	EXPECT_EQ(
		Utility::get_model("simple_test_model.gltf"),
		Utility::get_top_level_path()/"test/data/simple_test_model.gltf");
	EXPECT_EQ(
		Utility::get_texture("texture.jpg"),
		Utility::get_top_level_path()/"resources/default/textures/texture.jpg");
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
	EXPECT_NE(std::ranges::find(textures, "skybox/front.jpg"), textures.end());
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
	MaterialSystem materials;
	const auto make_texture = [&materials](const ETextureSemantic semantic)
	{
		auto texture = std::make_unique<TextureMaterial>();
		texture->semantic = semantic;
		return materials.add(std::move(texture));
	};
	const auto base = make_texture(ETextureSemantic::BASE_COLOR);
	const auto specular = make_texture(ETextureSemantic::SPECULAR);

	const std::vector<MaterialHandle> owners{ specular, base };
	const TexturedMatGroup group(owners);
	EXPECT_EQ(group.base_color_mat, base->get_id());
	EXPECT_FALSE(group.normal_mat);
	EXPECT_EQ(group.specular_mat, specular->get_id());
	EXPECT_EQ(group.get_materials(), (MatVec{
		base->get_id(), specular->get_id() }));
}

TEST(Basics, TexturedMaterialGroupRejectsDuplicateSemantics)
{
	MaterialSystem materials;
	auto first = std::make_unique<TextureMaterial>();
	first->semantic = ETextureSemantic::BASE_COLOR;
	auto second = std::make_unique<TextureMaterial>();
	second->semantic = ETextureSemantic::BASE_COLOR;
	const auto first_owner = materials.add(std::move(first));
	const auto second_owner = materials.add(std::move(second));

	const std::vector<MaterialHandle> owners{ first_owner, second_owner };
	EXPECT_THROW((void)TexturedMatGroup{ owners }, std::runtime_error);
}
