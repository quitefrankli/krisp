#pragma once

#include <filesystem>
#include <atomic>
#include <cstdint>
#include <exception>
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

	// fps defines the output time base. Each submitted frame advances by 1/fps.
	void start(const std::filesystem::path& path, uint32_t width, uint32_t height, int fps = 60);
	void submit_frame(const uint8_t* bgra, uint32_t width, uint32_t height);
	void stop();
	bool is_recording() const { return recording.load(std::memory_order_acquire); }

private:
	struct FrameData
	{
		std::vector<uint8_t> bgra;
		int64_t pts;
	};

	void encoder_loop();
	void flush_encoder();
	void encode_frame(const FrameData& frame);
	void release_resources();

	static constexpr size_t MAX_QUEUED_FRAMES = 2;

	std::atomic<bool> recording = false;
	AVFormatContext* fmt_ctx = nullptr;
	AVCodecContext* codec_ctx = nullptr;
	AVStream* stream = nullptr;
	AVFrame* av_frame = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* sws_ctx = nullptr;
	uint32_t frame_width = 0;
	uint32_t frame_height = 0;
	int64_t next_pts = 0;

	std::queue<FrameData> frame_queue;
	std::mutex queue_mutex;
	std::condition_variable queue_cv;
	std::condition_variable queue_space_cv;
	std::thread encoder_thread;
	bool stop_requested = false;
	std::exception_ptr encoder_error;
};
