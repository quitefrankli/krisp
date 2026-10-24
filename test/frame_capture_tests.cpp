#include "frame_capture_session.hpp"
#include "frame_capture_pixels.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>


TEST(FrameCaptureSession, rejectsCompletionForWrongFrameAndCompletesIdentifiedFrame)
{
	FrameCaptureSession session;
	const auto request_id = session.request_next();
	std::optional<FrameCaptureSession::Target> target;
	std::thread publisher([&] { target = session.bind_and_wait(42); });
	while (!session.get_target())
		std::this_thread::yield();
	const auto bound = *session.get_target();

	FrameCaptureResult wrong{
		.request_id = request_id,
		.frame_number = 41,
		.width = 1,
		.height = 1,
		.pixels = {1, 2, 3, 4},
		.status = FrameCaptureStatus::Success,
	};
	EXPECT_FALSE(session.complete(bound, std::move(wrong)));
	EXPECT_FALSE(session.wait(request_id, std::chrono::milliseconds(1)));

	FrameCaptureResult correct{
		.request_id = request_id,
		.frame_number = 42,
		.width = 1,
		.height = 1,
		.pixels = {5, 6, 7, 8},
		.status = FrameCaptureStatus::Success,
	};
	EXPECT_TRUE(session.complete(bound, std::move(correct)));
	publisher.join();
	const auto result = session.wait(request_id, std::chrono::milliseconds(1));
	ASSERT_TRUE(result);
	EXPECT_EQ(result->frame_number, 42);
	EXPECT_EQ(result->pixels, (std::vector<uint8_t>{5, 6, 7, 8}));
	ASSERT_TRUE(session.take_result(request_id));
	const auto next_id = session.request_next();
	std::optional<FrameCaptureSession::Target> frame_zero;
	std::thread zero_publisher([&] { frame_zero = session.bind_and_wait(0); });
	while (!session.get_target())
		std::this_thread::yield();
	const auto zero_target = *session.get_target();
	EXPECT_EQ(zero_target.frame_number, 0);
	EXPECT_EQ(zero_target.request_id, next_id);
	FrameCaptureResult zero_result{
		.request_id = next_id,
		.frame_number = 0,
		.width = 1,
		.height = 1,
		.pixels = {9, 10, 11, 12},
		.status = FrameCaptureStatus::Success,
	};
	EXPECT_TRUE(session.complete(zero_target, std::move(zero_result)));
	zero_publisher.join();
}

TEST(FrameCaptureSession, cancellationAndShutdownResolveBlockedPublication)
{
	FrameCaptureSession session;
	const auto request_id = session.request_next();
	std::optional<FrameCaptureSession::Target> target;
	std::thread publisher([&] { target = session.bind_and_wait(17); });
	while (!session.get_target())
		std::this_thread::yield();
	EXPECT_TRUE(session.cancel(request_id));
	publisher.join();
	const auto result = session.wait(request_id, std::chrono::milliseconds(1));
	ASSERT_TRUE(result);
	EXPECT_EQ(result->status, FrameCaptureStatus::Cancelled);

	FrameCaptureSession shutdown_session;
	const auto shutdown_id = shutdown_session.request_next();
	std::thread shutdown_publisher([&] { shutdown_session.bind_and_wait(18); });
	while (!shutdown_session.get_target())
		std::this_thread::yield();
	shutdown_session.cancel_all();
	shutdown_publisher.join();
	EXPECT_EQ(shutdown_session.wait(shutdown_id, std::chrono::milliseconds(1))->status, FrameCaptureStatus::Cancelled);
}

TEST(FrameCaptureSession, deadlineResolvesPendingRequestAsFailure)
{
	FrameCaptureSession session;
	const auto request_id = session.request_next(std::chrono::milliseconds(100));
	const auto target = session.bind_and_wait(0);
	ASSERT_TRUE(target);
	const auto result = session.wait(request_id, std::chrono::milliseconds(1));
	ASSERT_TRUE(result);
	EXPECT_EQ(result->status, FrameCaptureStatus::Failed);
	EXPECT_EQ(result->frame_number, 0);
}

TEST(FrameCaptureSession, malformedPixelPayloadResolvesAsFailure)
{
	FrameCaptureSession session;
	const auto request_id = session.request_next();
	std::optional<FrameCaptureSession::Target> target;
	std::thread publisher([&] { target = session.bind_and_wait(3); });
	while (!session.get_target())
		std::this_thread::yield();
	const auto bound = *session.get_target();
	FrameCaptureResult malformed{
		.request_id = request_id,
		.frame_number = 3,
		.width = 2,
		.height = 2,
		.pixels = {1, 2, 3, 4},
		.status = FrameCaptureStatus::Success,
	};
	EXPECT_FALSE(session.complete(bound, std::move(malformed)));
	publisher.join();
	const auto result = session.take_result(request_id);
	ASSERT_TRUE(result);
	EXPECT_EQ(result->status, FrameCaptureStatus::Failed);
}

TEST(FrameCapturePixels, convertsSupportedChannelOrdersWithoutChangingAlpha)
{
	const std::vector<uint8_t> bgra{3, 2, 1, 4, 30, 20, 10, 40};
	EXPECT_EQ(convert_capture_pixels_to_rgba8(bgra, CaptureChannelOrder::BGRA),
	          (std::vector<uint8_t>{1, 2, 3, 4, 10, 20, 30, 40}));
	EXPECT_EQ(convert_capture_pixels_to_rgba8(bgra, CaptureChannelOrder::RGBA), bgra);
	EXPECT_THROW(convert_capture_pixels_to_rgba8(std::span<const uint8_t>(bgra.data(), 3), CaptureChannelOrder::RGBA),
	             std::invalid_argument);
}
