#include "recording_session.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>


TEST(RecordingSession, is_inactive_until_started)
{
	RecordingSession session;

	EXPECT_FALSE(session.is_active());
	EXPECT_FALSE(session.begin_game_frame());
}

TEST(RecordingSession, publishes_fixed_delta_frames_in_sequence)
{
	RecordingSession session;
	session.start(30);

	const auto first = session.begin_game_frame();
	ASSERT_TRUE(first);
	EXPECT_EQ(first->sequence, 0u);
	EXPECT_FLOAT_EQ(first->delta_seconds, 1.0f / 30.0f);

	auto graphics = std::async(std::launch::async, [&session, expected = *first] {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		const auto target = session.get_capture_target();
		EXPECT_TRUE(target);
		if (target)
		{
			EXPECT_EQ(target->session, expected.session);
			EXPECT_EQ(target->sequence, expected.sequence);
			EXPECT_EQ(target->render_frame_number, 42u);
			EXPECT_TRUE(session.complete_capture(*target));
		}
	});

	EXPECT_TRUE(session.await_capture(*first, 42));
	graphics.get();

	const auto second = session.begin_game_frame();
	ASSERT_TRUE(second);
	EXPECT_EQ(second->sequence, 1u);
	EXPECT_FLOAT_EQ(second->delta_seconds, first->delta_seconds);
	session.stop();
}

TEST(RecordingSession, stop_releases_a_waiting_game_frame)
{
	RecordingSession session;
	session.start(60);
	const auto frame = session.begin_game_frame();
	ASSERT_TRUE(frame);

	auto graphics = std::async(std::launch::async, [&session] {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		session.stop();
	});

	EXPECT_FALSE(session.await_capture(*frame, 7));
	graphics.get();
	EXPECT_FALSE(session.is_active());
}

TEST(RecordingSession, suspension_discards_pending_capture_and_resume_continues_sequence)
{
	RecordingSession session;
	session.start(60);
	const auto first = session.begin_game_frame();
	ASSERT_TRUE(first);

	auto capture = std::async(std::launch::async, [&session, frame = *first] {
		return session.await_capture(frame, 12);
	});
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	while (!session.get_capture_target() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();
	const bool capture_target_published = session.get_capture_target().has_value();

	session.set_suspended(true);
	EXPECT_TRUE(capture_target_published);
	ASSERT_EQ(capture.wait_for(std::chrono::seconds(1)), std::future_status::ready);
	EXPECT_FALSE(capture.get());
	EXPECT_FALSE(session.get_capture_target());
	EXPECT_FALSE(session.begin_game_frame());

	session.set_suspended(false);
	const auto resumed = session.begin_game_frame();
	ASSERT_TRUE(resumed);
	EXPECT_EQ(resumed->sequence, 1u);
	auto resumed_capture = std::async(std::launch::async, [&session, frame = *resumed] {
		return session.await_capture(frame, 13);
	});
	const auto resumed_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	auto target = session.get_capture_target();
	while (!target && std::chrono::steady_clock::now() < resumed_deadline)
	{
		std::this_thread::yield();
		target = session.get_capture_target();
	}
	const bool resumed_target_published = target.has_value();
	if (target)
		EXPECT_TRUE(session.complete_capture(*target));
	else
		session.set_suspended(true);
	EXPECT_TRUE(resumed_target_published);
	EXPECT_EQ(resumed_capture.wait_for(std::chrono::seconds(1)), std::future_status::ready);
	if (resumed_capture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		EXPECT_EQ(resumed_capture.get(), resumed_target_published);
	session.stop();
}

TEST(RecordingSession, suspension_releases_a_paced_game_frame_waiter)
{
	RecordingSession session;
	session.start(1);
	ASSERT_TRUE(session.begin_game_frame());
	auto next_frame = std::async(std::launch::async, [&session] {
		return session.begin_game_frame();
	});
	EXPECT_EQ(next_frame.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);

	session.set_suspended(true);
	ASSERT_EQ(next_frame.wait_for(std::chrono::seconds(1)), std::future_status::ready);
	EXPECT_FALSE(next_frame.get());
	session.stop();
}

TEST(RecordingSession, a_new_session_rejects_an_old_ticket)
{
	RecordingSession session;
	session.start(60);
	const auto old_frame = session.begin_game_frame();
	ASSERT_TRUE(old_frame);
	session.stop();
	session.start(24);

	EXPECT_FALSE(session.await_capture(*old_frame, 1));
	const auto new_frame = session.begin_game_frame();
	ASSERT_TRUE(new_frame);
	EXPECT_EQ(new_frame->sequence, 0u);
	EXPECT_FLOAT_EQ(new_frame->delta_seconds, 1.0f / 24.0f);
	session.stop();
}
