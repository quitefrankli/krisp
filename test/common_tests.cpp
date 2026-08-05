#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>
#include <renderable/composited_texture_material.hpp>
#include <renderable/render_types.hpp>
#include <entity_component_system/material_system.hpp>
#include <analytics.hpp>
#include <utility.hpp>

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <limits>
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


TEST(Basics, PbrMaterialDefaultsAndMoveAreDeterministic)
{
	PbrMaterial material;
	EXPECT_TRUE(glm_equal(material.data.base_color_factor, glm::vec4(1.0f)));
	EXPECT_FLOAT_EQ(material.data.metallic_factor, 1.0f);
	EXPECT_FLOAT_EQ(material.data.roughness_factor, 1.0f);

	material.data.roughness_factor = 0.25f;
	std::unique_ptr<Material> ptr = std::make_unique<PbrMaterial>(std::move(material));
	const auto& moved = static_cast<const PbrMaterial&>(*ptr);
	EXPECT_FLOAT_EQ(moved.data.roughness_factor, 0.25f);
}

TEST(Basics, PbrMaterialRejectsInvalidFactors)
{
	EXPECT_THROW((void)PbrMaterial(glm::vec4(-0.1f), 0.0f, 1.0f), std::invalid_argument);
	EXPECT_THROW((void)PbrMaterial(glm::vec4(1.0f), 1.1f, 1.0f), std::invalid_argument);
	EXPECT_THROW((void)PbrMaterial(
		glm::vec4(std::numeric_limits<float>::quiet_NaN()), 0.0f, 1.0f), std::invalid_argument);
}

TEST(Basics, SkinnedRenderTypeClassification)
{
	EXPECT_TRUE(is_skinned_render_type(ERenderType::SKINNED));
	EXPECT_TRUE(is_skinned_render_type(ERenderType::SKINNED_COLOR));
	EXPECT_FALSE(is_skinned_render_type(ERenderType::COLOR));
	EXPECT_FALSE(is_skinned_render_type(ERenderType::STANDARD));
}

TEST(Basics, CompositedTextureMaterialSupportsSeveralLayersAndRejectsNesting)
{
	MaterialSystem materials;
	std::vector<MaterialHandle> sources;
	for (int index = 0; index < 3; ++index)
	{
		auto source = std::make_unique<TextureMaterial>();
		source->width = 16;
		source->height = 8;
		sources.push_back(materials.add(std::move(source)));
	}

	constexpr uint32_t expected_max_layers = 64;
	std::vector<TextureCompositionLayer> layers;
	layers.reserve(expected_max_layers);
	for (uint32_t index = 0; index < expected_max_layers; ++index)
		layers.push_back({ .source = sources[index % sources.size()] });
	auto composition = std::make_unique<CompositedTextureMaterial>(16, 8, layers);
	EXPECT_EQ(composition->layers.size(), expected_max_layers);
	EXPECT_EQ(composition->width, 16u);
	EXPECT_EQ(composition->height, 8u);
	const auto composition_owner = materials.add(std::move(composition));

	EXPECT_THROW(
		(void)CompositedTextureMaterial(16, 8, {
			TextureCompositionLayer{ .source = composition_owner } }),
		std::invalid_argument);
	EXPECT_THROW(
		(void)CompositedTextureMaterial(8, 8, {
			TextureCompositionLayer{ .source = sources[0] } }),
		std::invalid_argument);
	EXPECT_THROW(
		(void)CompositedTextureMaterial(16, 8, {
			TextureCompositionLayer{ .source = sources[0], .scale = { 0.0f, 1.0f } } }),
		std::invalid_argument);
	layers.push_back({ .source = sources[0] });
	EXPECT_THROW((void)CompositedTextureMaterial(16, 8, layers), std::invalid_argument);
}
