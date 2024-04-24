#include <game_engine.hpp>
#include <iapplication.hpp>

#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"

#include <gtest/gtest.h>


class GameEngineTests : public testing::Test
{
public:
    GameEngineTests() : 
		engine(mock_window, [](GameEngine& engine) { return std::make_unique<MockGraphicsEngine>(engine); })
	{
		engine.set_application(&application);
	}

	MockWindow mock_window;
    DummyApplication application;
    GameEngine engine;
};

TEST_F(GameEngineTests, Constructor)
{
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
	engine.get_graphics_engine().increment_num_objs_deleted();
	engine.main_loop(1.0f);
	ASSERT_FALSE(engine.get_object(id));
}