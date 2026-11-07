#include <window_presentation_state.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <thread>

TEST(WindowPresentationStateTests, PublishesCoherentSnapshotsAndAdvancesGeneration)
{
	App::WindowPresentationState state;
	const auto initial = state.get();

	state.publish(1920, 1080, false);
	const auto resized = state.get();
	EXPECT_EQ(resized.framebuffer_width, 1920);
	EXPECT_EQ(resized.framebuffer_height, 1080);
	EXPECT_FALSE(resized.iconified);
	EXPECT_EQ(resized.generation, initial.generation + 1);

	state.publish(1920, 1080, false);
	EXPECT_EQ(state.get().generation, resized.generation);

	state.publish(0, 0, true);
	const auto minimised = state.get();
	EXPECT_EQ(minimised.framebuffer_width, 0);
	EXPECT_EQ(minimised.framebuffer_height, 0);
	EXPECT_TRUE(minimised.iconified);
	EXPECT_EQ(minimised.generation, resized.generation + 1);
}

TEST(WindowPresentationStateTests, WakesWaiterWhenPresentationChanges)
{
	App::WindowPresentationState state;
	std::atomic<bool> shutdown = false;
	const auto generation = state.get().generation;
	std::promise<void> waiting;
	auto waiting_started = waiting.get_future();
	auto result = std::async(std::launch::async, [&] {
		waiting.set_value();
		return state.wait_for_change(generation, shutdown);
	});
	waiting_started.wait();
	state.publish(800, 600, false);

	EXPECT_TRUE(result.get());
}

TEST(WindowPresentationStateTests, ShutdownWakeReturnsWithoutAChange)
{
	App::WindowPresentationState state;
	std::atomic<bool> shutdown = false;
	const auto generation = state.get().generation;
	std::promise<void> waiting;
	auto waiting_started = waiting.get_future();
	auto result = std::async(std::launch::async, [&] {
		waiting.set_value();
		return state.wait_for_change(generation, shutdown);
	});
	waiting_started.wait();
	shutdown.store(true, std::memory_order_release);
	state.wake_waiters();

	EXPECT_FALSE(result.get());
}
