#pragma once

#include <filesystem>
#include <cstdint>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class VideoRecorder
{
public:
	VideoRecorder() = default;
	~VideoRecorder();
	VideoRecorder(const VideoRecorder&) = delete;
	VideoRecorder& operator=(const VideoRecorder&) = delete;

	// fps is used for encoder hints; actual frame timestamps are wall-clock driven
	void start(const std::filesystem::path& path, uint32_t width, uint32_t height, int fps = 60);
	void submit_frame(const uint8_t* bgra, uint32_t width, uint32_t height);
	void stop();
	bool is_recording() const { return recording; }

private:
	struct FrameData
	{
		std::vector<uint8_t> bgra;
		int64_t pts_90k;
	};

	void encoder_loop();
	void flush_encoder();
	void encode_frame(const FrameData& frame);

	bool recording = false;
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVStream* stream = nullptr;
	AVFrame* av_frame = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* sws_ctx = nullptr;
	std::chrono::steady_clock::time_point record_start;

	std::queue<FrameData> frame_queue;
	std::mutex queue_mutex;
	std::condition_variable queue_cv;
	std::thread encoder_thread;
	bool stop_requested = false;
};
