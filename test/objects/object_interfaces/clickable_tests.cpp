#include <objects/object_interfaces/clickable.hpp>

#include <gtest/gtest.h>
#include <glm/gtc/epsilon.hpp>

#include <vector>


class MockClickableObject : public Object, public IClickable
{
public:
	MockClickableObject() : IClickable(static_cast<Object&>(*this)) {}

	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override
	{
		// simple collision assume object is a single point
		// R_o + lambda * R_d = P
		// lambda = (P - R_o) / R_d

		// requires some special handling for vector division due to 0
		const glm::vec3 lambda = (get_position() - ray.origin + glm::vec3(0.0001f)) / (ray.direction + glm::vec3(0.0001f));
		// std::cout<<glm::to_string(lambda)<<glm::to_string(get_position() - ray.origin + glm::vec3(0.0001f))<<glm::to_string(ray.direction + glm::vec3(0.0001f))<<'\n';
		const float epsilon = 0.001f;
		if (glm::epsilonEqual(lambda.x, lambda.y, epsilon) && glm::epsilonEqual(lambda.y, lambda.z, epsilon))
		{
			if (lambda.x < 0)
			{
				return false;
			}

			intersection = get_position();
			return true;
		}

		return false;
	}
};

class ClickableTestFixture : public testing::Test
{
public:
	ClickableTestFixture()
	{
		clickables.emplace_back(std::make_unique<MockClickableObject>());
		clickables.emplace_back(std::make_unique<MockClickableObject>());
		clickables.emplace_back(std::make_unique<MockClickableObject>());
		clickables[0]->set_position({-1.0f, 0.0f, 0.0f});
		clickables[1]->set_position({0.0f, 0.0f, 1.0f});
		clickables[2]->set_position({1.0f, 0.0f, 0.0f});
	}

	std::vector<std::unique_ptr<MockClickableObject>> clickables;
};

TEST_F(ClickableTestFixture, interface_check_click_simple)
{
	Maths::Ray ray({}, Maths::forward_vec);
	glm::vec3 intersection;
	ASSERT_FALSE(clickables[0]->check_click(ray, intersection));
	ASSERT_TRUE(clickables[1]->check_click(ray, intersection));
	ASSERT_FALSE(clickables[2]->check_click(ray, intersection));
}

TEST_F(ClickableTestFixture, interface_check_click_angled)
{
	Maths::Ray ray(glm::vec3(-1.0f, 0.0f, 0.0f), glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)));
	glm::vec3 intersection;
	// ASSERT_TRUE(clickables[0]->check_click(ray, intersection));
	// ASSERT_TRUE(clickables[1]->check_click(ray, intersection));
	ASSERT_FALSE(clickables[2]->check_click(ray, intersection));
}

class MockDispatcher1 : public OnClickDispatchers::IBaseDispatcher
{
public:
	virtual void dispatch_on_click(Object& object, const Maths::Ray& ray, const glm::vec3& intersection) override
	{
		throw std::runtime_error("");
	}
};

class MockDispatcher2 : public OnClickDispatchers::IBaseDispatcher
{
public:
	virtual void dispatch_on_click(Object& object, const Maths::Ray& ray, const glm::vec3& intersection) override
	{
		invocations_counter++;
	}

	int invocations_counter = 0;
};

TEST_F(ClickableTestFixture, check_double_dispatch_multiple_modes)
{
	// Clickable objects use double dispatch, the click_mode provides the behaviour after clicking
	MockDispatcher1 dis1;
	MockDispatcher2 dis2;
	EXPECT_THROW(clickables[0]->on_click(dis1, Maths::Ray({}, {}), glm::vec3{}), std::runtime_error);
	clickables[0]->on_click(dis2, Maths::Ray({}, {}), glm::vec3{});
	ASSERT_EQ(dis2.invocations_counter, 1);
}