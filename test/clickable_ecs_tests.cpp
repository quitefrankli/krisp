#include "test_helper.hpp"

#include <entity_component_system/ecs.hpp>

#include <gtest/gtest.h>

#include <vector>


class ClickableECSFixture : public testing::Test
{
public:
	ClickableECSFixture()
	{
		ecs.add_object(object1);
		ecs.add_object(object2);
		
		object2.set_position(glm::vec3(1.0f, 1.0f, 1.0f));

		ecs.add_collider(object1.get_id(), std::make_unique<SphereCollider>(Maths::Sphere{}));
		ecs.add_collider(object2.get_id(), std::make_unique<SphereCollider>(Maths::Sphere{}));

		ecs.add_clickable_entity(object1.get_id());
		ecs.add_clickable_entity(object2.get_id());
	}

	ECS ecs;
	Object object1;
	Object object2;
};

TEST_F(ClickableECSFixture, hit_obj1)
{
	Maths::Ray ray{-Maths::forward_vec, Maths::forward_vec};
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(0.0f, 0.0f, -0.5f)));

	ray.origin = Maths::up_vec * 5.0f;
	ray.direction = -Maths::up_vec;
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(0.0f, 0.5f, 0.0f)));
}

TEST_F(ClickableECSFixture, hit_obj2)
{
	Maths::Ray ray{ glm::vec3(1.0f, 1.0f, -5.0f), Maths::forward_vec };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(1.0f, 1.0f, 0.5f)));

	ray.origin = glm::vec3(1.0f, -1.0f, 1.0f);
	ray.direction = Maths::up_vec;
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, glm::vec3(1.0f, 0.5f, 1.0f)));
}

TEST_F(ClickableECSFixture, hit_none)
{	
	Maths::Ray ray{ glm::vec3(-1.0f, 0.0f, -5.0f), Maths::forward_vec };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_FALSE(res.bCollided);
}

TEST_F(ClickableECSFixture, hit_both)
{
	Maths::Ray ray{ glm::vec3(-1.0f), glm::vec3(1.0f) };
	auto res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object1.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, 0.5f * glm::normalize(glm::vec3(-1.0f))));

	ray.origin = glm::vec3(2.0f);
	ray.direction = glm::vec3(-1.0f);
	res = ecs.check_any_entity_clicked(ray);
	ASSERT_TRUE(res.bCollided);
	ASSERT_EQ(res.id, object2.get_id());
	ASSERT_TRUE(glm_equal(res.intersection, 1.0f + 0.5f * glm::normalize(glm::vec3(1.0f))));
}