#include "video_recorder.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavutil/error.h>
}

namespace
{
std::runtime_error av_error(const std::string& operation, const int result)
{
	char message[AV_ERROR_MAX_STRING_SIZE]{};
	av_strerror(result, message, sizeof(message));
	return std::runtime_error("VideoRecorder: " + operation + ": " + message);
}

void require_av_success(const std::string& operation, const int result)
{
	if (result < 0)
		throw av_error(operation, result);
}
}

VideoRecorder::~VideoRecorder()
{
	try
	{
		stop();
	}
	catch (...)
	{
		// Destructors cannot report an encoder failure. Explicit stop() does.
	}
}

void VideoRecorder::start(
	const std::filesystem::path& path,
	const uint32_t width,
	const uint32_t height,
	const int fps)
{
	if (is_recording())
		return;
	if (width == 0 || height == 0)
		throw std::invalid_argument("VideoRecorder: frame dimensions must be positive");
	if (fps <= 0)
		throw std::invalid_argument("VideoRecorder: frame rate must be positive");

	try
	{
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path());
		const std::string path_str = path.string();

		avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, path_str.c_str());
		if (!fmt_ctx)
			throw std::runtime_error("VideoRecorder: failed to allocate output context");

		const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
		if (!codec)
			throw std::runtime_error("VideoRecorder: libx264 encoder not found");

		stream = avformat_new_stream(fmt_ctx, nullptr);
		if (!stream)
			throw std::runtime_error("VideoRecorder: failed to create stream");
		stream->id = 0;
		stream->avg_frame_rate = {fps, 1};

		codec_ctx = avcodec_alloc_context3(codec);
		if (!codec_ctx)
			throw std::runtime_error("VideoRecorder: failed to allocate codec context");

		codec_ctx->codec_id = AV_CODEC_ID_H264;
		codec_ctx->width = static_cast<int>(width);
		codec_ctx->height = static_cast<int>(height);
		codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		codec_ctx->time_base = {1, fps};
		codec_ctx->framerate = {fps, 1};
		codec_ctx->gop_size = fps;
		codec_ctx->max_b_frames = 2;

		if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		require_av_success("failed to set encoder preset",
			av_opt_set(codec_ctx->priv_data, "preset", "fast", 0));
		require_av_success("failed to set encoder quality",
			av_opt_set(codec_ctx->priv_data, "crf", "23", 0));
		require_av_success("failed to open codec", avcodec_open2(codec_ctx, codec, nullptr));
		require_av_success("failed to copy codec parameters",
			avcodec_parameters_from_context(stream->codecpar, codec_ctx));

		stream->time_base = codec_ctx->time_base;
		if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
			require_av_success("failed to open output file",
				avio_open(&fmt_ctx->pb, path_str.c_str(), AVIO_FLAG_WRITE));
		require_av_success("failed to write header", avformat_write_header(fmt_ctx, nullptr));

		av_frame = av_frame_alloc();
		if (!av_frame)
			throw std::runtime_error("VideoRecorder: failed to allocate frame");
		av_frame->format = AV_PIX_FMT_YUV420P;
		av_frame->width = static_cast<int>(width);
		av_frame->height = static_cast<int>(height);
		require_av_success("failed to allocate frame buffer", av_frame_get_buffer(av_frame, 0));

		packet = av_packet_alloc();
		if (!packet)
			throw std::runtime_error("VideoRecorder: failed to allocate packet");

		sws_ctx = sws_getContext(
			static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_BGRA,
			static_cast<int>(width), static_cast<int>(height), AV_PIX_FMT_YUV420P,
			SWS_BILINEAR, nullptr, nullptr, nullptr);
		if (!sws_ctx)
			throw std::runtime_error("VideoRecorder: failed to create swscale context");

		{
			const std::lock_guard lock(queue_mutex);
			frame_queue = {};
			stop_requested = false;
			encoder_error = nullptr;
			next_pts = 0;
		}
		frame_width = width;
		frame_height = height;
		recording.store(true, std::memory_order_release);
		encoder_thread = std::thread(&VideoRecorder::encoder_loop, this);
	}
	catch (...)
	{
		recording.store(false, std::memory_order_release);
		if (encoder_thread.joinable())
		{
			{
				const std::lock_guard lock(queue_mutex);
				stop_requested = true;
			}
			queue_cv.notify_all();
			encoder_thread.join();
		}
		release_resources();
		throw;
	}
}

void VideoRecorder::submit_frame(
	const uint8_t* bgra, const uint32_t width, const uint32_t height)
{
	if (!is_recording())
		return;
	if (!bgra)
		throw std::invalid_argument("VideoRecorder: frame data is empty");
	if (width != frame_width || height != frame_height)
		throw std::invalid_argument("VideoRecorder: frame dimensions changed during recording");

	FrameData frame;
	frame.bgra.assign(bgra, bgra + static_cast<size_t>(width) * height * 4);

	{
		std::unique_lock lock(queue_mutex);
		queue_space_cv.wait(lock, [this] {
			return frame_queue.size() < MAX_QUEUED_FRAMES
				|| stop_requested || encoder_error || !is_recording();
		});
		if (encoder_error)
			std::rethrow_exception(encoder_error);
		if (stop_requested || !is_recording())
			return;

		frame.pts = next_pts++;
		frame_queue.push(std::move(frame));
	}
	queue_cv.notify_one();
}

void VideoRecorder::encode_frame(const FrameData& frame)
{
	require_av_success("failed to make frame writable", av_frame_make_writable(av_frame));

	const uint8_t* src_slices[1] = {frame.bgra.data()};
	const int src_stride[1] = {av_frame->width * 4};
	if (sws_scale(
			sws_ctx, src_slices, src_stride, 0, av_frame->height,
			av_frame->data, av_frame->linesize) <= 0)
		throw std::runtime_error("VideoRecorder: failed to convert frame pixels");

	av_frame->pts = frame.pts;
	av_frame->duration = 1;
	require_av_success("failed to submit frame to encoder",
		avcodec_send_frame(codec_ctx, av_frame));

	while (true)
	{
		const int result = avcodec_receive_packet(codec_ctx, packet);
		if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
			break;
		require_av_success("failed to receive encoded packet", result);

		if (packet->duration == 0)
			packet->duration = 1;
		av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
		packet->stream_index = stream->index;
		const int write_result = av_interleaved_write_frame(fmt_ctx, packet);
		av_packet_unref(packet);
		require_av_success("failed to write encoded packet", write_result);
	}
}

void VideoRecorder::encoder_loop()
{
	try
	{
		while (true)
		{
			std::unique_lock lock(queue_mutex);
			queue_cv.wait(lock, [this] { return !frame_queue.empty() || stop_requested; });
			if (frame_queue.empty())
				break;

			FrameData frame = std::move(frame_queue.front());
			frame_queue.pop();
			lock.unlock();
			queue_space_cv.notify_one();
			encode_frame(frame);
		}

		flush_encoder();
	}
	catch (...)
	{
		const std::lock_guard lock(queue_mutex);
		encoder_error = std::current_exception();
		stop_requested = true;
		frame_queue = {};
		queue_space_cv.notify_all();
	}
}

void VideoRecorder::flush_encoder()
{
	require_av_success("failed to flush encoder", avcodec_send_frame(codec_ctx, nullptr));
	while (true)
	{
		const int result = avcodec_receive_packet(codec_ctx, packet);
		if (result == AVERROR_EOF || result == AVERROR(EAGAIN))
			break;
		require_av_success("failed to receive flushed packet", result);

		if (packet->duration == 0)
			packet->duration = 1;
		av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
		packet->stream_index = stream->index;
		const int write_result = av_interleaved_write_frame(fmt_ctx, packet);
		av_packet_unref(packet);
		require_av_success("failed to write flushed packet", write_result);
	}
}

void VideoRecorder::stop()
{
	if (!recording.exchange(false, std::memory_order_acq_rel))
		return;

	{
		const std::lock_guard lock(queue_mutex);
		stop_requested = true;
	}
	queue_cv.notify_all();
	queue_space_cv.notify_all();
	if (encoder_thread.joinable())
		encoder_thread.join();

	std::exception_ptr failure;
	{
		const std::lock_guard lock(queue_mutex);
		failure = encoder_error;
	}
	if (!failure)
	{
		const int result = av_write_trailer(fmt_ctx);
		if (result < 0)
			failure = std::make_exception_ptr(av_error("failed to write trailer", result));
	}

	release_resources();
	{
		const std::lock_guard lock(queue_mutex);
		frame_queue = {};
		encoder_error = nullptr;
	}
	if (failure)
		std::rethrow_exception(failure);
}

void VideoRecorder::release_resources()
{
	av_frame_free(&av_frame);
	av_packet_free(&packet);
	avcodec_free_context(&codec_ctx);
	if (sws_ctx)
		sws_freeContext(sws_ctx);
	sws_ctx = nullptr;

	if (fmt_ctx)
	{
		if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE) && fmt_ctx->pb)
			avio_closep(&fmt_ctx->pb);
		avformat_free_context(fmt_ctx);
	}
	fmt_ctx = nullptr;
	stream = nullptr;
	frame_width = 0;
	frame_height = 0;
}
