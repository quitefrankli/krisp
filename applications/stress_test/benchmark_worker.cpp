#include "benchmark_worker.hpp"
#include "benchmark_measurement.hpp"
#include "benchmark_scene.hpp"

#include <camera.hpp>
#include <config.hpp>
#include <constants.hpp>
#include <graphics_engine/point_lighting.hpp>
#include <entity_component_system/light_source.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>
#include <resource_paths.hpp>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <array>
#include <atomic>
#include <cmath>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <thread>

namespace
{
using Json = nlohmann::json;
using Path = std::filesystem::path;

struct WorkerOptions
{
	std::string scenario;
	std::string view = "atrium";
	std::string mode;
	int count = 100;
	double warmup = 10.0;
	double measure = 30.0;
	Path cache;
	Path output;
};

Json read_json(const Path& path)
{
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Cannot read " + path.string());
	return Json::parse(input);
}

void write_json(const Path& path, const Json& value)
{
	const Path temporary = path.string() + ".tmp";
	{
		std::ofstream output(temporary);
		output.exceptions(std::ios::badbit | std::ios::failbit);
		output << value.dump(2) << '\n';
	}
	std::filesystem::rename(temporary, path);
}

glm::vec3 vector3(const Json& value)
{
	if (!value.is_array() || value.size() != 3) throw std::runtime_error("Expected a three-component vector");
	return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

const char* kind_name(RuntimeSampleKind kind)
{
	switch (kind)
	{
#define SAMPLE_NAME(name) case RuntimeSampleKind::name: return #name
		SAMPLE_NAME(Simulation); SAMPLE_NAME(Graphics); SAMPLE_NAME(FrameFenceWait);
		SAMPLE_NAME(ImageFenceWait); SAMPLE_NAME(Acquire); SAMPLE_NAME(Submit);
		SAMPLE_NAME(Present); SAMPLE_NAME(GameSleep); SAMPLE_NAME(GraphicsSleep);
		SAMPLE_NAME(GameDeadline); SAMPLE_NAME(GraphicsDeadline); SAMPLE_NAME(Gpu);
#undef SAMPLE_NAME
	}
	throw std::logic_error("Unknown runtime sample kind");
}

Json graphics_json(const RuntimeGraphicsInfo& info, const RuntimeOptions& options)
{
	return {
		{"width", info.width}, {"height", info.height},
		{"requested_width", info.requested_width}, {"requested_height", info.requested_height},
		{"vendor_id", info.vendor_id}, {"device_id", info.device_id}, {"device_name", info.device_name},
		{"driver_version", info.driver_version}, {"api_version", info.api_version},
		{"validation_enabled", info.validation_enabled}, {"timestamp_valid_bits", info.timestamp_valid_bits},
		{"timestamp_period_ns", info.timestamp_period_ns}, {"swapchain_images", info.swapchain_images},
		{"presentation_mode", info.presentation_mode}, {"requested_presentation_mode", info.requested_presentation_mode},
		{"configuration_note", info.configuration_note}, {"gpu_unavailable_reason", info.gpu_unavailable_reason},
		{"vertex_buffer_bytes", info.vertex_buffer_bytes}, {"index_buffer_bytes", info.index_buffer_bytes},
		{"simulation_hz", 60}, {"fixed_simulation_timestep", true},
		{"render_hz", options.render_hz.value_or(0.0)}, {"msaa_samples", CSTS::MSAA_SAMPLE_COUNT},
		{"shadow_resolution", POINT_SHADOW_RESOLUTION},
		{"render_path", "rasterized-pbr"}, {"exposure_ev", 0.0}, {"ui", false}, {"performance_overlay", true}, {"interactive", false}
	};
}

struct WorkerState
{
	std::atomic<uint64_t> start_ns{0};
	std::atomic<uint64_t> end_ns{0};
	std::atomic<uint64_t> pending_gpu{0};
	std::atomic<bool> collector_failed{false};
	Json graphics;
	Json scenario;
	std::vector<std::string> errors;
	uint64_t ready_ns = 0;
	uint64_t first_measured_tick = 0;
	uint64_t completed_ns = 0;
	bool completed = false;
};

// Only the consumer thread serializes records. Frame producers copy bounded value records.
class SampleWriter
{
public:
	SampleWriter(std::shared_ptr<RuntimeObservation> observation, WorkerState& state, const Path& output) :
		observation(std::move(observation)), state(state), path(output), thread([this](std::stop_token stop) { consume(stop); }) {}

	~SampleWriter() { finish(); }
	void finish()
	{
		if (thread.joinable()) { thread.request_stop(); thread.join(); }
	}

private:
	void consume(std::stop_token stop)
	{
		try
		{
			std::ofstream output(path);
			output.exceptions(std::ios::badbit | std::ios::failbit);
			std::set<uint64_t> submitted;
			for (;;)
			{
				for (const auto& sample : observation->drain())
				{
					const auto start = state.start_ns.load();
					const auto end = state.end_ns.load();
					const auto membership = BenchmarkMeasurement::classify(sample, start, end, submitted.contains(sample.submission_id));
					if (membership.included && sample.kind == RuntimeSampleKind::Submit && sample.success)
						submitted.insert(sample.submission_id);
					if (sample.kind == RuntimeSampleKind::Gpu)
						submitted.erase(sample.submission_id);
					Json record{
						{"kind", kind_name(sample.kind)}, {"begin_ns", sample.begin_ns}, {"end_ns", sample.end_ns},
						{"duration_ns", sample.kind == RuntimeSampleKind::Gpu ? sample.duration_ns : sample.end_ns - sample.begin_ns}, {"submission_id", sample.submission_id},
						{"frame_number", sample.frame_number}, {"tick_index", sample.tick_index},
						{"publication_ns", sample.publication_ns}, {"missed_slots", sample.missed_slots},
						{"success", sample.success}, {"phase", membership.phase}, {"included", membership.included},
						{"exclusion_reason", membership.exclusion_reason ? Json(membership.exclusion_reason) : Json(nullptr)}
					};
					if (sample.kind == RuntimeSampleKind::GameDeadline || sample.kind == RuntimeSampleKind::GraphicsDeadline)
						record["deadline_ns"] = sample.begin_ns;
					output << record.dump() << '\n';
				}
				state.pending_gpu.store(submitted.size());
				if (stop.stop_requested()) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			output.flush();
		}
		catch (...) { state.collector_failed.store(true); }
	}

	std::shared_ptr<RuntimeObservation> observation;
	WorkerState& state;
	Path path;
	std::jthread thread;
};

class Workload final : public DummyApplication
{
public:
	Workload(WorkerOptions options, WorkerState& state, Json assets) : options(std::move(options)), state(state), assets(std::move(assets)) {}
	bool allows_playerless_normal_mode() const override { return true; }

	void on_begin(GameEngine& engine) override
	{
		engine.set_exposure_ev(0.0f);
		engine.set_normal_mode_cursor_captured(false);
		if (options.scenario == "textured-objects" || options.scenario == "textured-environment")
			spawn_imported(engine);
		else
			spawn_cubes(engine);
		engine.set_game_mode(EGameMode::NORMAL);
		if (engine.get_ecs().get_renderable_ids().size() != state.scenario.at("renderables").get<size_t>())
			throw std::runtime_error("Scene renderable count differs from the workload definition");
		state.scenario["name"] = options.scenario;
		state.scenario["version"] = 1;
		state.scenario["count"] = options.count;
		state.scenario["view"] = options.view;
		state.scenario["mode"] = options.mode;
	}

	void on_tick(GameEngine& engine, float) override
	{
		if (!engine.is_ready()) return;
		if (state.collector_failed.load() || engine.get_runtime_options().observation->lost_records())
		{
			invalidate("Observation collection failed or overflowed");
			engine.shutdown();
			return;
		}
		const auto now = runtime_now_ns();
		if (!state.ready_ns)
		{
			glfwFocusWindow(engine.get_window().get_glfw_window());
			state.ready_ns = now;
			const auto start = now + static_cast<uint64_t>(options.warmup * 1e9);
			state.start_ns.store(start);
			state.end_ns.store(start + static_cast<uint64_t>(options.measure * 1e9));
			write_json(options.output / "ready.json", {{"status", "ready"}, {"ready_ns", now}});
		}
		const auto start = state.start_ns.load();
		const auto end = state.end_ns.load();
		if (now >= start && now < end)
		{
			if (!first_measured_tick)
			{
				first_measured_tick = engine.get_simulation_tick();
				state.first_measured_tick = *first_measured_tick;
			}
			int width, height;
			const auto window = engine.get_window().get_glfw_window();
			glfwGetFramebufferSize(window, &width, &height);
			if (width != 1280 || height != 720) invalidate("Framebuffer extent changed");
			if (!glfwGetWindowAttrib(window, GLFW_FOCUSED)) invalidate("Window lost focus");
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) invalidate("Window was minimised");
		}
		if (options.scenario == "moving-transforms" && now < end)
		{
			const auto tick = engine.get_simulation_tick() - first_measured_tick.value_or(0);
			const auto rotation = BenchmarkScene::rotation_for_tick(tick);
			for (const auto id : moving) engine.get_ecs().get_transformation(id).set_rotation(rotation);
		}
		if (now >= end)
		{
			if (now - end < 200'000'000) return;
			if (state.graphics.at("timestamp_valid_bits").get<uint32_t>() && state.pending_gpu.load()
				&& now - end < 30'000'000'000ULL) return;
			state.completed = true;
			state.completed_ns = now;
			engine.shutdown();
		}
	}

private:
	void invalidate(const std::string& reason)
	{
		if (std::find(state.errors.begin(), state.errors.end(), reason) == state.errors.end()) state.errors.push_back(reason);
	}

	void light(GameEngine& engine, glm::vec3 position, float intensity, bool shadows)
	{
		auto& object = engine.spawn_object<Object>();
		engine.get_ecs().get_transformation(object.get_id()).set_position(position);
		engine.get_ecs().add_light_source(object.get_id(), LightComponent{.intensity = intensity, .casts_shadow = shadows});
	}

	void spawn_cubes(GameEngine& engine)
	{
		auto& ecs = engine.get_ecs();
		const auto mesh = ecs.get_mesh_system().add(MeshFactory::cube());
		std::array<MaterialHandle, 8> materials;
		for (size_t i = 0; i < materials.size(); ++i)
			materials[i] = ecs.get_material_system().add(std::make_unique<PbrMaterial>(
				glm::vec4(0.2f + static_cast<float>(i) * 0.08f, 0.4f, 0.65f, 1.0f), 0.0f, 0.6f));
		for (int i = 0; i < options.count; ++i)
		{
			auto& object = engine.spawn_object<Object>();
			const auto id = object.get_id();
			engine.attach_renderable(id, Renderable{.name = "Measured primitive", .mesh_owner = mesh,
				.material_owners = {materials[i % materials.size()]}});
			auto& transform = ecs.get_transformation(id);
			transform.set_position(BenchmarkScene::cube_position(i));
			transform.set_scale(glm::vec3(0.3f));
			moving.push_back(id);
		}
		auto& floor = engine.spawn_object<Object>();
		engine.attach_renderable(floor.get_id(), Renderable{.name = "Measured floor", .casts_shadow = false,
			.mesh_owner = mesh, .material_owners = {materials.front()}});
		auto& transform = ecs.get_transformation(floor.get_id());
		transform.set_position({0.0f, -0.2f, 0.0f});
		transform.set_scale({18.0f, 0.1f, 18.0f});
		light(engine, {0.0f, 32.0f, 0.0f}, 5120.0f, options.scenario == "point-shadows");
		engine.get_camera().look_at(glm::vec3(0.0f), {0.0f, 32.0f, -32.0f});
		if (!BenchmarkScene::cube_grid_in_frustum(engine.get_camera().get_projection() * engine.get_camera().get_view()))
			throw std::runtime_error("The maximum cube workload does not fit the camera frustum");
		state.scenario = {{"instances", options.count}, {"renderables", options.count + 1}, {"materials", 8}, {"meshes", 1}};
	}

	void spawn_imported(GameEngine& engine)
	{
		const bool environment = options.scenario == "textured-environment";
		const auto& asset = assets.at("assets").at(environment ? "sponza" : "flight_helmet");
		const auto registered = engine.assets().load_asset(asset.at("gltf").get<std::string>());
		const auto model = engine.assets().instantiate_model(engine.get_ecs(), AssetRegistry::default_assembly(registered));
		if (!model.warnings.empty()) throw std::runtime_error("Imported workload produced unexpected warnings");
		size_t renderables = 0;
		for (const auto& part : model.meshes) renderables += part.renderables.size();
		if (renderables != asset.at("renderables_per_instance").get<size_t>())
			throw std::runtime_error("Imported workload differs from pinned renderable count");
		const int instances = environment ? 1 : options.count;
		for (int i = 0; i < instances; ++i)
		{
			auto& object = engine.spawn_object<Object>();
			auto& transform = engine.get_ecs().get_transformation(object.get_id());
			transform.set_transform(model.onload_transform.get_mat4());
			transform.set_scale(glm::vec3(asset.value("import_scale", 1.0f)));
			if (!environment) transform.set_position({(static_cast<float>(i % 8) - 3.5f) * 1.5f, 0.0f, (static_cast<float>(i / 8) - 3.5f) * 1.5f});
			for (const auto& part : model.meshes)
				for (const auto& renderable : part.renderables) engine.attach_renderable(object.get_id(), renderable);
		}
		if (environment)
		{
			const auto& view = asset.at("views").at(options.view);
			engine.get_camera().look_at(vector3(view.at("focus")), vector3(view.at("from")));
			light(engine, vector3(asset.at("light_position")), asset.at("light_intensity").get<float>(), false);
		}
		else
		{
			engine.get_camera().look_at({0.0f, 1.0f, 0.0f}, {0.0f, 9.0f, -12.0f});
			light(engine, {0.0f, 10.0f, -3.0f}, 800.0f, false);
		}
		state.scenario = {{"instances", instances}, {"renderables", renderables * instances}, {"asset", asset}, {"profile", assets.at("profile")}};
	}

	WorkerOptions options;
	WorkerState& state;
	Json assets;
	std::vector<ObjectID> moving;
	std::optional<uint64_t> first_measured_tick;
};

template<class T>
T parse_number(const std::string& value)
{
	T result{};
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
	if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
		throw std::invalid_argument("Invalid numeric worker argument: " + value);
	return result;
}

WorkerOptions parse_options(int argc, char** argv)
{
	WorkerOptions options;
	for (int i = 2; i < argc; i += 2)
	{
		if (i + 1 >= argc) throw std::invalid_argument("Worker option needs a value");
		const std::string key(argv[i]);
		const std::string value(argv[i + 1]);
		if (key == "--scenario") options.scenario = value;
		else if (key == "--view") options.view = value;
		else if (key == "--mode") options.mode = value;
		else if (key == "--count") options.count = parse_number<int>(value);
		else if (key == "--warmup") options.warmup = parse_number<double>(value);
		else if (key == "--measure") options.measure = parse_number<double>(value);
		else if (key == "--asset-cache") options.cache = value;
		else if (key == "--output") options.output = value;
		else throw std::invalid_argument("Unknown worker argument " + key);
	}
	const std::set<std::string> scenarios{"static-objects", "moving-transforms", "point-shadows", "textured-objects", "textured-environment"};
	if (!scenarios.contains(options.scenario) || (options.mode != "throughput" && options.mode != "paced")
		|| options.count < 1 || options.count > 1000 || options.output.empty()
		|| !std::isfinite(options.warmup) || options.warmup < 0.0 || options.warmup > 3600.0
		|| !std::isfinite(options.measure) || options.measure <= 0.0 || options.measure > 3600.0)
		throw std::invalid_argument("Invalid worker configuration");
	return options;
}
}

int run_benchmark_worker(int argc, char** argv)
{
	WorkerOptions options;
	WorkerState state;
	try
	{
		options = parse_options(argc, argv);
		std::filesystem::create_directories(options.output);
		const auto assets = read_json(options.cache / "prepared.json");
		auto paths = ResourcePaths::paths_for_executable(PROJECT_NAME);
		paths.app_resources = options.cache;
		Config::init(PROJECT_NAME, std::move(paths));
		auto observation = std::make_shared<RuntimeObservation>(65536);
		RuntimeOptions runtime;
		runtime.vertex_buffer_bytes = 128ULL * 1024 * 1024;
		runtime.index_buffer_bytes = 64ULL * 1024 * 1024;
		runtime.width = 1280;
		runtime.height = 720;
		runtime.simulation_hz = 60.0;
		runtime.fixed_simulation_timestep = true;
		runtime.render_hz = options.mode == "paced" ? 60.0 : 0.0;
		runtime.presentation = options.mode == "paced" ? PresentationPreference::Paced : PresentationPreference::Uncapped;
		runtime.interactive = false;
		runtime.ui = false;
		runtime.performance_overlay = true;
		runtime.observation = observation;
		observation->start();
		SampleWriter writer(observation, state, options.output / "raw.jsonl");
		{
			auto engine = GameEngine::create_with_options<Workload>(runtime, options, state, assets);
			state.graphics = graphics_json(engine.get_graphics_engine().get_runtime_graphics_info(), runtime);
			if (state.graphics.at("width") != 1280 || state.graphics.at("height") != 720)
				throw std::runtime_error("The requested framebuffer extent is unavailable");
			engine.run();
			engine.get_window().poll_events();
			int width, height;
			glfwGetFramebufferSize(engine.get_window().get_glfw_window(), &width, &height);
			const bool resized = width != 1280 || height != 720;
			if (resized) state.errors.push_back("Framebuffer extent changed");
			if (engine.get_run_failure())
			{
				if (!resized) std::rethrow_exception(engine.get_run_failure());
				try { std::rethrow_exception(engine.get_run_failure()); }
				catch (const std::exception& error) { state.errors.push_back(std::string("Graphics stopped after resize: ") + error.what()); }
			}
		}
		state.scenario["first_measured_tick"] = state.first_measured_tick;
		observation->stop();
		writer.finish();
		if (!state.completed) state.errors.push_back("Worker stopped before measurement completion");
		if (state.collector_failed.load()) state.errors.push_back("Raw sample writer failed");
		if (observation->lost_records()) state.errors.push_back("Observation buffer overflow");
		const bool gpu_supported = state.graphics.at("timestamp_valid_bits").get<uint32_t>() > 0;
		if (gpu_supported && state.pending_gpu.load()) state.errors.push_back("GPU observations incomplete");
		const std::string status = !state.errors.empty() ? "invalid" : gpu_supported ? "complete" : "partial";
		write_json(options.output / "worker.json", {
			{"schema_version", 1}, {"status", status}, {"window_start_ns", state.start_ns.load()},
			{"window_end_ns", state.end_ns.load()}, {"gpu_supported", gpu_supported},
			{"lost_records", observation->lost_records()}, {"graphics", state.graphics}, {"scenario", state.scenario},
			{"errors", state.errors}, {"phases", {{"ready_ns", state.ready_ns}, {"completed_ns", state.completed_ns},
				{"warmup_seconds", options.warmup}, {"measure_seconds", options.measure}}}
		});
		return state.errors.empty() ? 0 : 1;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Worker failed: " << error.what() << '\n';
		if (!options.output.empty())
		{
			try { write_json(options.output / "worker.json", {{"schema_version", 1}, {"status", "failed"}, {"errors", {error.what()}}}); }
			catch (...) {}
		}
		return 1;
	}
}
