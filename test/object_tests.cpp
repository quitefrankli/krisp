#include "test_helper.hpp"

#include <objects/object.hpp>
#include <serialization/serializer.hpp>

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

TEST(ObjectSerialization, round_trips_common_state_and_exact_id)
{
	Object source;
	source.set_name("saved object");
	source.set_visibility(false);
	source.set_relative_position({ 1.0f, 2.0f, 3.0f });
	source.set_relative_scale({ 2.0f, 3.0f, 4.0f });
	source.set_aabb(AABB({ -1.0f, -2.0f, -3.0f }, { 4.0f, 5.0f, 6.0f }));
	source.renderables.push_back(Renderable{
		MeshID(41), { MaterialID(42), MaterialID(43) }, ERenderType::STANDARD,
		EAlphaMode::MASK, 0.25f, 0.75f, false, true });

	Serializer serializer;
	source.serialize(serializer);
	Object restored;
	restored.deserialize(Deserializer::parse(serializer.emit()));

	EXPECT_EQ(restored.get_id(), source.get_id());
	EXPECT_EQ(restored.get_name(), "saved object");
	EXPECT_FALSE(restored.get_visibility());
	EXPECT_TRUE(glm_equal(restored.get_relative_position(), glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_EQ(restored.renderables.size(), 1);
	EXPECT_EQ(restored.renderables[0].mesh_id, MeshID(41));
	EXPECT_EQ(restored.renderables[0].material_ids, (MatVec{ MaterialID(42), MaterialID(43) }));
	EXPECT_EQ(restored.renderables[0].alpha_mode, EAlphaMode::MASK);
	EXPECT_FALSE(restored.renderables[0].casts_shadow);
	EXPECT_TRUE(restored.renderables[0].render_on_top);
}
