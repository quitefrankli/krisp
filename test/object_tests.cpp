#include "test_helper.hpp"

#include <objects/object.hpp>
#include <serialization/serializer.hpp>
#include <serialization/serialization_helpers.hpp>

#include <gtest/gtest.h>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>


class ObjectTests : public testing::Test
{
public:
    ObjectTests()
    {
		obj1.set_position(Maths::zero_vec);
		obj2.set_position({1.0f, 0.0f, 0.0f});
		obj2.attach_to(&obj1);
    }

	Object obj1;
	Object obj2;
};

class InspectableObject : public Object
{
public:
	bool is_attached() const { return parent != nullptr; }
	size_t child_count() const { return children.size(); }
};

TEST_F(ObjectTests, initialisation_test)
{
	ASSERT_TRUE(glm_equal(obj1.get_position(), Maths::zero_vec));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(obj1.get_scale(), Maths::identity_vec));

	ASSERT_TRUE(glm_equal(obj2.get_position(), {1.0f, 0.0f, 0.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(obj2.get_scale(), Maths::identity_vec));
}

TEST_F(ObjectTests, parenting_translation)
{
	obj1.set_position({1.0f, 2.0f, -3.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {2.0f, 2.0f, -3.0f}));

	obj2.set_position({3.0f, 3.0f, 3.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {3.0f, 3.0f, 3.0f}));

	obj2.set_relative_position({0.0f, 0.0f, -2.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {1.0f, 2.0f, -5.0f}));
}

TEST_F(ObjectTests, parenting_rotation)
{
	auto quat = glm::angleAxis(Maths::PI/2.0f, Maths::up_vec);
	obj1.set_rotation(quat);
	ASSERT_TRUE(glm_equal(obj1.get_position(), Maths::zero_vec));
	ASSERT_TRUE(glm_equal(obj2.get_position(), glm::vec3(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat));

	auto quat2 = glm::angleAxis(Maths::PI/2.0f, Maths::right_vec);
	obj2.set_rotation(quat2);
	ASSERT_TRUE(glm_equal(obj1.get_position(), Maths::zero_vec));
	ASSERT_TRUE(glm_equal(obj2.get_position(), glm::vec3(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat2));
	ASSERT_TRUE(glm_equal(obj2.get_relative_rotation(), glm::inverse(quat) * quat2));

	obj2.set_relative_rotation(quat2);
	ASSERT_TRUE(glm_equal(obj1.get_position(), Maths::zero_vec));
	ASSERT_TRUE(glm_equal(obj2.get_position(), glm::vec3(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat * quat2));
	ASSERT_TRUE(glm_equal(obj2.get_relative_rotation(), quat2));
}

TEST_F(ObjectTests, parenting_translation_and_rotation)
{
	auto quat = glm::angleAxis(Maths::PI/2.0f, Maths::up_vec);
	obj1.set_rotation(quat);
	obj1.set_position({1.0f, 2.0f, -3.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {1.0f, 2.0f, -4.0f}));

	obj2.set_relative_position({0.0f, 0.0f, -1.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {0.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat));
}

// Test for triple parent hierarchy
TEST_F(ObjectTests, parenting_translation_and_rotation_triple_hierarchy)
{
	Object obj3;
	obj3.set_position({1.0f, 1.0f, 1.0f});
	obj3.attach_to(&obj2);

	obj1.set_position({1.0f, 2.0f, -3.0f});
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {2.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj3.get_position(), {2.0f, 3.0f, -2.0f}));

	auto quat = glm::angleAxis(Maths::PI/2.0f, Maths::up_vec);
	obj1.set_rotation(quat);
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {1.0f, 2.0f, -4.0f}));
	ASSERT_TRUE(glm_equal(obj3.get_position(), {2.0f, 3.0f, -4.0f}));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj3.get_rotation(), quat));

	auto quat2 = glm::angleAxis(Maths::PI/2.0f, Maths::right_vec);
	obj2.set_relative_rotation(quat2);
	ASSERT_TRUE(glm_equal(obj1.get_position(), {1.0f, 2.0f, -3.0f}));
	ASSERT_TRUE(glm_equal(obj2.get_position(), {1.0f, 2.0f, -4.0f}));
	ASSERT_TRUE(glm_equal(obj3.get_position(), {2.0f, 1.0f, -4.0f}));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), quat));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), quat*quat2));
	ASSERT_TRUE(glm_equal(obj3.get_rotation(), quat*quat2));
}

TEST_F(ObjectTests, scale_does_not_affect_rotation)
{
	const auto quat = glm::angleAxis(Maths::PI/2.0f, Maths::up_vec);
	const auto scale = glm::vec3(2.0f, 2.0f, 2.0f);
	obj2.set_rotation(quat);
	const auto orig_pos = obj2.get_position();
	const auto orig_rot = obj2.get_rotation();
	obj2.set_scale(scale);
	ASSERT_TRUE(glm_equal(obj2.get_position(), orig_pos));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), orig_rot));
	ASSERT_TRUE(glm_equal(obj2.get_scale(), scale));
}

TEST(ObjectTestsMisc, attaching_preserves_original_transform)
{
	Object obj1;
	Object obj2;
	const glm::vec3 orig_pos = {1.0f, 2.0f, -3.0f};
	const glm::quat orig_rot = glm::angleAxis(Maths::PI/2.0f, Maths::up_vec);
	const glm::vec3 orig_scale = {2.0f, 2.0f, 2.0f};
	obj1.set_position(orig_pos);
	obj1.set_rotation(orig_rot);
	obj1.set_scale(orig_scale);
	obj1.attach_to(&obj2);
	
	ASSERT_TRUE(glm_equal(obj1.get_position(), orig_pos));
	ASSERT_TRUE(glm_equal(obj1.get_rotation(), orig_rot));
	ASSERT_TRUE(glm_equal(obj1.get_scale(), orig_scale));
	ASSERT_TRUE(glm_equal(obj2.get_position(), Maths::zero_vec));
	ASSERT_TRUE(glm_equal(obj2.get_rotation(), Maths::identity_quat));
	ASSERT_TRUE(glm_equal(obj2.get_scale(), Maths::identity_vec));
}

TEST(ObjectTestsMisc, destroying_parent_detaches_children)
{
	auto parent = std::make_unique<InspectableObject>();
	InspectableObject child;
	child.set_position({ 1.0f, 2.0f, 3.0f });
	child.attach_to(parent.get());
	ASSERT_TRUE(child.is_attached());

	parent.reset();

	EXPECT_FALSE(child.is_attached());
	EXPECT_TRUE(glm_equal(child.get_position(), glm::vec3(1.0f, 2.0f, 3.0f)));
}

TEST(ObjectTestsMisc, destroying_child_removes_it_from_parent)
{
	InspectableObject parent;
	auto child = std::make_unique<InspectableObject>();
	child->attach_to(&parent);
	ASSERT_EQ(parent.child_count(), 1);

	child.reset();

	EXPECT_EQ(parent.child_count(), 0);
}

TEST(ObjectTestsMisc, rejects_cycles_through_any_descendant)
{
	InspectableObject root;
	InspectableObject child;
	InspectableObject grandchild;
	child.attach_to(&root);
	grandchild.attach_to(&child);

	root.attach_to(&grandchild);

	EXPECT_FALSE(root.is_attached());
	EXPECT_EQ(grandchild.child_count(), 0);
}

TEST(ObjectTestsMisc, moving_parent_repairs_child_backlinks)
{
	InspectableObject original_parent;
	InspectableObject child;
	child.attach_to(&original_parent);

	InspectableObject moved_parent(std::move(original_parent));
	moved_parent.set_position({ 2.0f, 0.0f, 0.0f });

	EXPECT_TRUE(child.is_attached());
	EXPECT_EQ(moved_parent.child_count(), 1);
	EXPECT_TRUE(glm_equal(child.get_position(), glm::vec3(2.0f, 0.0f, 0.0f)));
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
		auto bounds = serializer.map("aabb");
		Serialization::write_vec3(bounds, "min", Maths::zero_vec);
		Serialization::write_vec3(bounds, "max", Maths::zero_vec);
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
