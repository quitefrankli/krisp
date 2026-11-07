#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace App
{
	struct PresentationState
	{
		int framebuffer_width = 1400;
		int framebuffer_height = 800;
		bool iconified = false;
		std::uint64_t generation = 0;
	};

	class WindowPresentationState
	{
	public:
		PresentationState get() const
		{
			std::lock_guard lock(mutex);
			return state;
		}

		void publish(int framebuffer_width, int framebuffer_height, bool iconified)
		{
			{
				std::lock_guard lock(mutex);
				if (state.framebuffer_width == framebuffer_width &&
					state.framebuffer_height == framebuffer_height &&
					state.iconified == iconified)
					return;

				state.framebuffer_width = framebuffer_width;
				state.framebuffer_height = framebuffer_height;
				state.iconified = iconified;
				++state.generation;
			}
			changed.notify_all();
		}

		bool wait_for_change(std::uint64_t generation, const std::atomic<bool>& shutdown) const
		{
			std::unique_lock lock(mutex);
			changed.wait(lock, [&] {
				return state.generation != generation || shutdown.load(std::memory_order_acquire);
			});
			return state.generation != generation;
		}

		void wake_waiters() const
		{
			{
				std::lock_guard lock(mutex);
			}
			changed.notify_all();
		}

	private:
		mutable std::mutex mutex;
		mutable std::condition_variable changed;
		PresentationState state;
	};
}
