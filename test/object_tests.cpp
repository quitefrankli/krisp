#include "test_helper.hpp"

#include <objects/object.hpp>
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
}

TEST(ObjectSerialization, rejects_procedural_resources)
{
	Object source;
	auto mesh_owner = MeshSystem::add(std::make_unique<ColorMesh>(
		ColorVertices{ SDS::ColorVertex{} }, VertexIndices{ 0 }));
	source.renderables.push_back(Renderable{
		.pipeline_render_type = ERenderType::STANDARD, .mesh_owner = mesh_owner });

	Serializer serializer;
	EXPECT_THROW(source.serialize(serializer), SerializationError);
}

TEST(ObjectSerialization, rejects_procedural_resources_during_deserialization)
{
	const auto legacy_object = [](const bool procedural_mesh, const bool procedural_material)
	{
		Serializer serializer;
		serializer.write("id", 1);
		serializer.write("name", "legacy object");
		serializer.write("visible", true);
		Serialization::write_transform(serializer, "world_transform", Maths::Transform{});
		Serialization::write_transform(serializer, "relative_transform", Maths::Transform{});
		auto saved = serializer.sequence("renderables").append_map();
		if (procedural_mesh)
			saved.write("mesh_id", 41);
		else
		{
			auto mesh_source = saved.map("mesh_source");
			mesh_source.write("path", "model.gltf");
			mesh_source.write("scene", 0);
			mesh_source.write("node", 0);
			mesh_source.write("primitive", 0);
		}
		auto materials = saved.sequence("material_ids");
		if (procedural_material)
			materials.append(42);
		saved.write("render_type", static_cast<int>(ERenderType::STANDARD));
		saved.write("alpha_mode", static_cast<int>(EAlphaMode::OPAQUE));
		saved.write("alpha_cutoff", 0.5f);
		saved.write("opacity", 1.0f);
		saved.write("casts_shadow", true);
		saved.write("render_on_top", false);
		Serialization::write_transform(saved, "local_transform", Maths::Transform{});
		return serializer.emit();
	};

	Object restored;
	EXPECT_THROW(
		restored.deserialize(Deserializer::parse(legacy_object(true, false))),
		SerializationError);
	EXPECT_THROW(
		restored.deserialize(Deserializer::parse(legacy_object(false, true))),
		SerializationError);
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

TEST(ObjectRenderableFrameID, packs_without_cross_object_collision)
{
	const ObjectRenderableFrameID last_for_first_object(
		ObjectID(1), CSTS::MAX_RENDERABLES_PER_OBJECT - 1, CSTS::UPPERBOUND_SWAPCHAIN_IMAGES - 1);
	const ObjectRenderableFrameID first_for_next_object(ObjectID(2), 0, 0);
	EXPECT_EQ(
		last_for_first_object.get_underlying() + 1,
		first_for_next_object.get_underlying());
}
