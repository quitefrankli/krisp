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
