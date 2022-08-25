#include <game_engine.ipp>
#include <game_engine_callbacks.ipp>
#include <interface/gizmo.ipp>
#include <input.ipp>

#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"

#include <gtest/gtest.h>


class TestFixture : public testing::Test
{
public:
    TestFixture() : engine([](){}, mock_window)
    {
        engine.set_application(&application);
    }

    DummyApplication application;
	MockWindow mock_window;
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