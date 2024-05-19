#include "test_helper.hpp"

#include <objects/object.hpp>

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