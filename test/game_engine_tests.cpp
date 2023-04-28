#include <game_engine.ipp>
#include <game_engine_callbacks.ipp>
#include <interface/gizmo.ipp>
#include <input.ipp>

#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"

#include <gtest/gtest.h>


class GameEngineTests : public testing::Test
{
public:
    GameEngineTests() : 
		engine([](){}, mock_window)
    {
        engine.set_application(&application);
    }

	MockWindow mock_window;
    DummyApplication application;
    GameEngine<MockGraphicsEngine> engine;
};

TEST_F(GameEngineTests, Constructor)
{
    ASSERT_TRUE(engine.get_light_source());
}

TEST_F(GameEngineTests, main_loop)
{
    engine.main_loop(1.0f);
}

TEST_F(GameEngineTests, spawning_and_deleting_objects)
{
	auto& obj = engine.spawn_object(std::make_shared<Object>());
	const auto id = obj.get_id();
	ASSERT_TRUE(engine.get_object(id));

	engine.delete_object(id);
	ASSERT_FALSE(engine.get_object(id));
}

TEST_F(GameEngineTests, clickable_objects)
{
	auto& obj = engine.spawn_object<GenericClickableObject>(ShapeFactory::circle());
	auto& clickable = dynamic_cast<IClickable&>(obj);
	const auto id = obj.get_id();
	engine.add_clickable(id, &clickable);

	bool contains_clckable = false;
	const std::function<bool(IClickable*)> callback = [&clickable, &contains_clckable](IClickable* c)
	{
		if (&clickable == c)
		{
			contains_clckable = true;
		}
		return true;
	};
	engine.execute_on_clickables(callback);
	ASSERT_TRUE(contains_clckable);

	engine.delete_object(id);
	ASSERT_FALSE(engine.get_object(id));
	contains_clckable = false;
	engine.execute_on_clickables(callback);
	ASSERT_FALSE(contains_clckable);
}