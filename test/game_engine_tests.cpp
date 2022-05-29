#include <game_engine.ipp>
#include <game_engine_callbacks.ipp>
#include <interface/gizmo.ipp>
#include <window.ipp>
#include <input.ipp>

#include "mock_graphics_engine.hpp"

#include <gtest/gtest.h>


// template specialisation for mocked game engine
template<>
App::Window<GameEngine<MockGraphicsEngine>>::Window(GameEngine<MockGraphicsEngine>* game_engine)
{
	this->game_engine = game_engine;
}

class TestFixture : public testing::Test
{
public:
    TestFixture() : engine([](){})
    {
        engine.set_application(&application);
    }

    DummyApplication application;
    GameEngine<MockGraphicsEngine> engine;
};

TEST_F(TestFixture, Constructor)
{
    ASSERT_TRUE(engine.get_light_source());
}

TEST_F(TestFixture, main_loop)
{
    engine.main_loop(1.0f);
}