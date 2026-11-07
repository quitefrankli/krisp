#include "config.hpp"
#include "game_engine.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "gui/gui_manager.hpp"
#include "iapplication.hpp"
#include "window.hpp"
#include "utility.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <ctime>
#include <vector>

struct LifecycleMeasurements
{
	std::shared_ptr<FrameCaptureSession> capture;
	FrameCaptureSession::RequestId capture_request = 0;
	uint32_t baseline_allocations = 0;
	uint64_t minimised_ns = 0;
	uint64_t restored_ns = 0;
	std::clock_t minimised_cpu = 0;
	std::vector<uint64_t> resize_requests;
};

// Explicit graphics integration executable: not part of headless unit tests.
class LifecycleApplication final : public DummyApplication
{
public:
	LifecycleApplication(bool close_minimised, LifecycleMeasurements& measurements)
		: close_minimised(close_minimised), measurements(measurements) {}
	void on_begin(GameEngine&) override { start = std::chrono::steady_clock::now(); }
	void on_tick(GameEngine& engine, float) override
	{
		const auto elapsed = std::chrono::steady_clock::now() - start;
		if (elapsed < std::chrono::milliseconds(500 * (phase + 1))) return;
		auto* window = engine.get_window().get_glfw_window();
		std::cout << "Lifecycle phase " << phase << '\n';
		if (phase == 0 || phase == 1 || phase == 7 || phase == 8)
			measurements.resize_requests.push_back(runtime_now_ns());
		switch (phase++)
		{
		case 0:
			engine.get_gui_manager().debug.request_screenshot();
			glfwSetWindowSize(window, 960, 540); break;
		case 1:
			if (measurements.capture)
				measurements.capture_request = measurements.capture->request_next();
			else glfwSetWindowSize(window, 640, 800);
			break;
		case 2:
			if (measurements.capture)
			{
				const auto result = measurements.capture->take_result(measurements.capture_request);
				if (!result || result->status != FrameCaptureStatus::Success || result->pixels.empty())
					throw std::runtime_error("Post-resize presentation capture did not complete");
				std::cout << "Post-resize capture " << result->width << 'x' << result->height << '\n';
			}
			glfwMaximizeWindow(window); break;
		case 3: glfwRestoreWindow(window); break;
		case 4: engine.get_gui_manager().debug.request_recording_toggle(); break;
		case 5:
			measurements.minimised_ns = runtime_now_ns();
			measurements.minimised_cpu = std::clock();
			glfwIconifyWindow(window); break;
		case 6:
			engine.get_gui_manager().debug.request_screenshot();
			if (measurements.capture)
			{
				const auto id = measurements.capture->request_next();
				const auto result = measurements.capture->take_result(id);
				if (!result || result->status != FrameCaptureStatus::Failed)
					throw std::runtime_error("Capture during minimisation was not rejected");
			}
			measurements.restored_ns = runtime_now_ns();
			std::cout << "Minimised interval process CPU ms "
				<< 1000.0 * (std::clock() - measurements.minimised_cpu) / CLOCKS_PER_SEC << '\n';
			if (!engine.get_window().get_presentation_state().iconified)
				throw std::runtime_error("Window manager did not iconify window");
			if (close_minimised) engine.shutdown();
			else { glfwRestoreWindow(window); glfwShowWindow(window); glfwFocusWindow(window); }
			break;
		case 7:
			if (engine.get_window().get_presentation_state().iconified)
				throw std::runtime_error("Native restore did not complete");
			glfwSetWindowSize(window, 800, 600); break;
		case 8: glfwSetWindowSize(window, 1280, 720); break;
		case 9: {
			VmaTotalStatistics statistics{};
			vmaCalculateStatistics(static_cast<GraphicsEngine&>(engine.get_graphics_engine()).get_allocator(), &statistics);
			measurements.baseline_allocations = statistics.total.statistics.allocationCount;
			glfwSetWindowSize(window, 800, 600); break;
		}
		case 10: glfwSetWindowSize(window, 1280, 720); break;
		case 11: {
			VmaTotalStatistics statistics{};
			vmaCalculateStatistics(static_cast<GraphicsEngine&>(engine.get_graphics_engine()).get_allocator(), &statistics);
			std::cout << "GPU allocation count before/after resize cycle "
				<< measurements.baseline_allocations << '/' << statistics.total.statistics.allocationCount << '\n';
			if (statistics.total.statistics.allocationCount > measurements.baseline_allocations)
				throw std::runtime_error("GPU allocations grew after resize-return cycle");
			engine.shutdown(); break;
		}
		}
	}
private:
	LifecycleMeasurements& measurements;
	bool close_minimised;
	int phase = 0;
	std::chrono::steady_clock::time_point start;
};

class StartupWindow final : public App::Window
{
public:
	explicit StartupWindow(bool deferred) : deferred(deferred) {}
	App::PresentationState get_presentation_state() const override
	{
		auto state = App::Window::get_presentation_state();
		// Model a surface with no initial drawable pixels; restoration exercises
		// the zero-to-actual image-count rebuild through the real Vulkan renderer.
		if (deferred && !shown) { state.framebuffer_width = 0; state.framebuffer_height = 0; }
		return state;
	}
	void show() override
	{
		shown = true;
		App::Window::show();
		if (deferred) return;
		glfwIconifyWindow(get_glfw_window());
		restore_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
	}
	void poll_events() override
	{
		App::Window::poll_events();
		if (!deferred && !restored && std::chrono::steady_clock::now() >= restore_at)
		{
			if (!get_presentation_state().iconified)
				throw std::runtime_error("Startup window did not minimise");
			glfwRestoreWindow(get_glfw_window());
			glfwShowWindow(get_glfw_window());
			glfwFocusWindow(get_glfw_window());
			restored = true;
			std::cout << "Restored during startup event processing\n";
		}
	}
private:
	bool deferred;
	bool shown = false;
	bool restored = false;
	std::chrono::steady_clock::time_point restore_at;
};

class LifecycleEngine final : public GameEngine
{
public:
	LifecycleEngine(std::unique_ptr<App::Window> window, std::unique_ptr<IApplication> application,
		std::unique_ptr<GraphicsEngineBase> graphics, RuntimeOptions options)
		: GameEngine(std::move(window), std::move(application), std::move(graphics), std::move(options)) {}
};

int main(int argc, char** argv)
{
	try
	{
		std::cout << std::unitbuf;
		Config::init("window-lifecycle-check");
		const std::string_view mode = argc > 1 ? argv[1] : "restore";
		std::unique_ptr<App::Window> window = (mode == "loading" || mode == "deferred")
			? std::make_unique<StartupWindow>(mode == "deferred") : std::make_unique<App::Window>();
		const auto screenshot_count = [] {
			std::size_t count = 0;
			const auto directory = Utility::get_writable_data_path() / "screenshots";
			if (std::filesystem::exists(directory))
				for (const auto& file : std::filesystem::directory_iterator(directory))
					if (file.path().extension() == ".png") ++count;
			return count;
		};
		const auto screenshots_before = screenshot_count();
		LifecycleMeasurements measurements;
		RuntimeOptions options;
		if (mode == "captures")
		{
			measurements.capture = std::make_shared<FrameCaptureSession>();
			options.frame_capture = measurements.capture;
		}
		options.observation = std::make_shared<RuntimeObservation>(65536);
		options.observation->start();
		auto graphics = std::make_unique<GraphicsEngine>(*window, options);
		LifecycleEngine engine(std::move(window),
			std::make_unique<LifecycleApplication>(mode == "close-minimised", measurements), std::move(graphics), options);
		engine.run();
		if (engine.get_run_failure()) std::rethrow_exception(engine.get_run_failure());
		const auto new_screenshots = screenshot_count() - screenshots_before;
		std::cout << "Completed screenshots " << new_screenshots << '\n';
		if (mode != "close-minimised" && new_screenshots < 2)
			throw std::runtime_error("Resize/restore screenshot requests did not complete");
		options.observation->stop();
		const auto samples = options.observation->drain();
		unsigned suspended_submissions = 0, suspended_ticks = 0;
		for (const auto& sample : samples)
			if (sample.begin_ns >= measurements.minimised_ns + 100000000
				&& sample.end_ns <= measurements.restored_ns && sample.success)
			{
				if (sample.kind == RuntimeSampleKind::Submit) ++suspended_submissions;
				if (sample.kind == RuntimeSampleKind::Simulation) ++suspended_ticks;
			}
		std::cout << "Settled minimised submissions " << suspended_submissions
			<< ", simulation ticks " << suspended_ticks << '\n';
		if (suspended_submissions || !suspended_ticks || options.observation->lost_records())
			throw std::runtime_error("Invalid suspension observations");
		for (auto requested : measurements.resize_requests)
			for (const auto& sample : samples)
				if (sample.kind == RuntimeSampleKind::Present && sample.success && sample.begin_ns >= requested)
				{
					std::cout << "Resize request to next present-call completion ms "
						<< (sample.end_ns - requested) / 1e6 << '\n';
					break;
				}
		return 0;
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
