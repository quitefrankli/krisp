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
    GameEngineTests() : engine([](){}, mock_window)
    {
        engine.set_application(&application);
    }

    DummyApplication application;
	MockWindow mock_window;
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