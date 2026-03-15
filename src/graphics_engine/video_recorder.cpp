#include "video_recorder.hpp"

#include <stdexcept>
#include <filesystem>

VideoRecorder::~VideoRecorder()
{
	if (recording)
	{
		stop();
	}
}

void VideoRecorder::start(const std::filesystem::path& path, uint32_t width, uint32_t height, int fps)
{
	if (recording)
	{
		return;
	}

	std::filesystem::create_directories(path.parent_path());
	const std::string path_str = path.string();

	avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, path_str.c_str());
	if (!fmt_ctx)
	{
		throw std::runtime_error("VideoRecorder: failed to allocate output context");
	}

	const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
	if (!codec)
	{
		throw std::runtime_error("VideoRecorder: libx264 encoder not found");
	}

	stream = avformat_new_stream(fmt_ctx, nullptr);
	if (!stream)
	{
		throw std::runtime_error("VideoRecorder: failed to create stream");
	}
	stream->id = 0;

	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx)
	{
		throw std::runtime_error("VideoRecorder: failed to allocate codec context");
	}

	// 90kHz time base — standard for MP4, fits microsecond wall-clock timestamps
	constexpr int TIMEBASE = 90000;

	codec_ctx->codec_id = AV_CODEC_ID_H264;
	codec_ctx->width = static_cast<int>(width);
	codec_ctx->height = static_cast<int>(height);
	codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	codec_ctx->time_base = {1, TIMEBASE};
	codec_ctx->framerate = {fps, 1};
	codec_ctx->gop_size = fps;
	codec_ctx->max_b_frames = 2;

	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	av_opt_set(codec_ctx->priv_data, "preset", "fast", 0);
	av_opt_set(codec_ctx->priv_data, "crf", "23", 0);

	if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
	{
		throw std::runtime_error("VideoRecorder: failed to open codec");
	}

	if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0)
	{
		throw std::runtime_error("VideoRecorder: failed to copy codec params");
	}

	stream->time_base = codec_ctx->time_base;

	if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&fmt_ctx->pb, path_str.c_str(), AVIO_FLAG_WRITE) < 0)
		{
			throw std::runtime_error("VideoRecorder: failed to open output file");
		}
	}

	if (avformat_write_header(fmt_ctx, nullptr) < 0)
	{
		throw std::runtime_error("VideoRecorder: failed to write header");
	}

	av_frame = av_frame_alloc();
	av_frame->format = AV_PIX_FMT_YUV420P;
	av_frame->width = static_cast<int>(width);
	av_frame->height = static_cast<int>(height);
	if (av_frame_get_buffer(av_frame, 0) < 0)
	{
		throw std::runtime_error("VideoRecorder: failed to allocate frame buffer");
	}

	packet = av_packet_alloc();

	sws_ctx = sws_getContext(
		static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_BGRA,
		static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!sws_ctx)
	{
		throw std::runtime_error("VideoRecorder: failed to create swscale context");
	}

	stop_requested = false;
	record_start = std::chrono::steady_clock::now();
	recording = true;
	encoder_thread = std::thread(&VideoRecorder::encoder_loop, this);
}

void VideoRecorder::submit_frame(const uint8_t* bgra, uint32_t width, uint32_t height)
{
	if (!recording)
	{
		return;
	}

	const auto elapsed = std::chrono::steady_clock::now() - record_start;
	const int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

	FrameData frame;
	frame.pts_90k = us * 90000 / 1000000;
	frame.bgra.assign(bgra, bgra + static_cast<size_t>(width) * height * 4);

	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		frame_queue.push(std::move(frame));
	}
	queue_cv.notify_one();
}

void VideoRecorder::encode_frame(const FrameData& frame)
{
	av_frame_make_writable(av_frame);

	const uint8_t* src_slices[1] = { frame.bgra.data() };
	const int src_stride[1] = { av_frame->width * 4 };
	sws_scale(sws_ctx, src_slices, src_stride, 0, av_frame->height, av_frame->data, av_frame->linesize);

	av_frame->pts = frame.pts_90k;

	if (avcodec_send_frame(codec_ctx, av_frame) < 0)
	{
		return;
	}

	while (avcodec_receive_packet(codec_ctx, packet) == 0)
	{
		av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
		packet->stream_index = stream->index;
		av_interleaved_write_frame(fmt_ctx, packet);
		av_packet_unref(packet);
	}
}

void VideoRecorder::encoder_loop()
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait(lock, [this] { return !frame_queue.empty() || stop_requested; });

		if (frame_queue.empty())
		{
			break;
		}

		FrameData frame = std::move(frame_queue.front());
		frame_queue.pop();
		lock.unlock();

		encode_frame(frame);
	}

	flush_encoder();
}

void VideoRecorder::flush_encoder()
{
	avcodec_send_frame(codec_ctx, nullptr);
	while (avcodec_receive_packet(codec_ctx, packet) == 0)
	{
		av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
		packet->stream_index = stream->index;
		av_interleaved_write_frame(fmt_ctx, packet);
		av_packet_unref(packet);
	}
}

void VideoRecorder::stop()
{
	if (!recording)
	{
		return;
	}
	recording = false;

	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		stop_requested = true;
	}
	queue_cv.notify_one();

	if (encoder_thread.joinable())
	{
		encoder_thread.join();
	}

	av_write_trailer(fmt_ctx);

	av_frame_free(&av_frame);
	av_packet_free(&packet);
	avcodec_free_context(&codec_ctx);
	sws_freeContext(sws_ctx);
	sws_ctx = nullptr;

	if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&fmt_ctx->pb);
	}
	avformat_free_context(fmt_ctx);
	fmt_ctx = nullptr;
	stream = nullptr;
}
