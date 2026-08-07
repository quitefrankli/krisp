#include "graphics_engine/video_recorder.hpp"

#include <gtest/gtest.h>

extern "C" {
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>

namespace
{
class TemporaryRecording
{
public:
	TemporaryRecording()
	{
		const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path()
			/ ("krisp_video_recorder_test_" + std::to_string(nonce) + ".mp4");
	}

	~TemporaryRecording()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::filesystem::path path;
};
}

TEST(VideoRecorder, writes_constant_rate_frame_timestamps)
{
	constexpr int fps = 30;
	constexpr uint32_t width = 16;
	constexpr uint32_t height = 16;
	constexpr int frame_count = 4;
	TemporaryRecording output;
	std::vector<uint8_t> pixels(width * height * 4, 0xff);

	VideoRecorder recorder;
	recorder.start(output.path, width, height, fps);
	for (int index = 0; index < frame_count; ++index)
	{
		pixels[0] = static_cast<uint8_t>(index * 40);
		recorder.submit_frame(pixels.data(), width, height);
	}
	recorder.stop();

	AVFormatContext* input = nullptr;
	ASSERT_GE(avformat_open_input(&input, output.path.c_str(), nullptr, nullptr), 0);
	ASSERT_NE(input, nullptr);
	ASSERT_GE(avformat_find_stream_info(input, nullptr), 0);
	const int stream_index = av_find_best_stream(
		input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	ASSERT_GE(stream_index, 0);
	const AVStream* stream = input->streams[stream_index];
	EXPECT_EQ(av_cmp_q(stream->avg_frame_rate, AVRational{fps, 1}), 0);

	std::vector<int64_t> timestamps;
	AVPacket* packet = av_packet_alloc();
	ASSERT_NE(packet, nullptr);
	while (av_read_frame(input, packet) >= 0)
	{
		if (packet->stream_index == stream_index && packet->pts != AV_NOPTS_VALUE)
			timestamps.push_back(av_rescale_q(
				packet->pts, stream->time_base, AVRational{1, fps}));
		av_packet_unref(packet);
	}
	av_packet_free(&packet);
	avformat_close_input(&input);

	std::ranges::sort(timestamps);
	ASSERT_EQ(timestamps.size(), frame_count);
	for (int index = 0; index < frame_count; ++index)
		EXPECT_EQ(timestamps[index], index);
}

TEST(VideoRecorder, rejects_invalid_configuration_and_frame_dimensions)
{
	TemporaryRecording output;
	VideoRecorder recorder;
	EXPECT_THROW(recorder.start(output.path, 0, 16, 60), std::invalid_argument);
	EXPECT_THROW(recorder.start(output.path, 16, 16, 0), std::invalid_argument);

	std::vector<uint8_t> pixels(16 * 16 * 4, 0);
	recorder.start(output.path, 16, 16, 60);
	EXPECT_THROW(recorder.submit_frame(pixels.data(), 8, 16), std::invalid_argument);
	recorder.stop();
}
