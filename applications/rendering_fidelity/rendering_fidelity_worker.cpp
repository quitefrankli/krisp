#include "rendering_fidelity_worker.hpp"

#include <camera.hpp>
#include <config.hpp>
#include <constants.hpp>
#include <entity_component_system/light_source.hpp>
#include <frame_capture_png.hpp>
#include <frame_capture_session.hpp>
#include <graphics_engine/point_lighting.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <objects/object.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>
#include <resource_loader/asset_registry.hpp>
#include <resource_loader/resource_loader.hpp>
#include <resource_paths.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/utsname.h>
#include <utility>
#include <vector>

namespace
{
using Json = nlohmann::json;
using Path = std::filesystem::path;

constexpr std::string_view build_class =
#ifdef NDEBUG
	"release";
#else
	"debug";
#endif

constexpr std::uint64_t vertex_buffer_capacity = 128ULL * 1024 * 1024;
constexpr std::uint64_t index_buffer_capacity = 64ULL * 1024 * 1024;

struct WorkerOptions
{
	std::string case_id;
	std::string capture_id;
	Path output;
	Path asset_cache;
};

void write_json(const Path &path, const Json &value)
{
	const Path temporary = path.string() + ".tmp";
	{
		std::ofstream output(temporary);
		output.exceptions(std::ios::badbit | std::ios::failbit);
		output << value.dump(2) << '\n';
	}
	std::filesystem::rename(temporary, path);
}

WorkerOptions parse_options(int argc, char **argv)
{
	WorkerOptions options;
	for (int index = 2; index < argc;)
	{
		const std::string key(argv[index]);
		if (index + 1 >= argc)
			throw std::invalid_argument("Worker option needs a value: " + key);
		const std::string value(argv[index + 1]);
		if (key == "--case")
			options.case_id = value;
		else if (key == "--capture")
			options.capture_id = value;
		else if (key == "--output")
			options.output = value;
		else if (key == "--asset-cache")
			options.asset_cache = value;
		else
			throw std::invalid_argument("Unknown worker option " + key);
		index += 2;
	}
	if (options.case_id.empty() || options.capture_id.empty() || options.output.empty())
		throw std::invalid_argument("Worker requires --case, --capture, and --output");
	return options;
}

Path fixture_root() { return Path(KRISP_SOURCE_DIR) / "applications/rendering_fidelity/fixtures"; }

MaterialHandle pbr(ECS &ecs, glm::vec4 colour, float metal = 0.0f, float rough = 0.5f,
                   PbrMaterial::TextureSlots textures = {}, PbrMaterial::Properties properties = {})
{
	return ecs.get_material_system().add(
		std::make_unique<PbrMaterial>(colour, metal, rough, std::move(textures), 1.0f, properties));
}

glm::vec3 vector3(const Json &value)
{
	if (!value.is_array() || value.size() != 3)
		throw std::runtime_error("Expected a three-component fixture vector");
	return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

Object &add_renderable(GameEngine &engine, const MeshHandle &mesh, const MaterialHandle &material,
                       const glm::vec3 &position, const glm::vec3 &scale, std::string name = "fidelity fixture",
                       EShadingMode shading_mode = EShadingMode::LIT, std::vector<MaterialHandle> texture_owners = {})
{
	auto &object = engine.spawn_object<Object>();
	auto &transform = engine.get_ecs().get_transformation(object.get_id());
	transform.set_position(position);
	transform.set_scale(scale);
	std::vector<MaterialHandle> material_owners;
	material_owners.reserve(1 + texture_owners.size());
	material_owners.push_back(material);
	material_owners.insert(material_owners.end(), texture_owners.begin(), texture_owners.end());
	const auto &mesh_data = mesh->get();
	const auto render_type =
		dynamic_cast<const TexMesh *>(&mesh_data) ? ERenderType::STANDARD
		: dynamic_cast<const ColorMesh *>(&mesh_data)
			? ERenderType::COLOR
			: throw std::invalid_argument("Fidelity fixture mesh has an unsupported vertex layout");
	engine.attach_renderable(object.get_id(), Renderable{.name = std::move(name),
	                                                     .pipeline_render_type = render_type,
	                                                     .shading_mode = shading_mode,
	                                                     .mesh_owner = mesh,
	                                                     .material_owners = std::move(material_owners)});
	return object;
}

void add_light(GameEngine &engine, glm::vec3 position, float intensity, glm::vec3 colour, bool shadows)
{
	auto &object = engine.spawn_object<Object>();
	engine.get_ecs().get_transformation(object.get_id()).set_position(position);
	engine.get_ecs().add_light_source(
		object.get_id(),
		LightComponent{.intensity = intensity, .color = colour, .enabled = true, .casts_shadow = shadows});
}

std::vector<unsigned char> read_bytes(const Path &path)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
		throw std::runtime_error("Missing fixture file " + path.string());
	return {std::istreambuf_iterator<char>(input), {}};
}

MaterialHandle texture(ECS &ecs, const std::string &filename, ETextureSemantic semantic)
{
	const auto path = fixture_root() / "textures" / filename;
	const auto bytes = read_bytes(path);
	return ResourceLoader::load_texture_bytes(ecs.get_material_system(), bytes, path.string(), semantic);
}

MeshPtr make_textured_sphere()
{
	constexpr uint32_t sectors = 32;
	constexpr uint32_t stacks = 16;
	TexVertices vertices;
	VertexIndices indices;
	vertices.reserve((sectors + 1) * (stacks + 1));
	indices.reserve(sectors * stacks * 6);
	for (uint32_t stack = 0; stack <= stacks; ++stack)
	{
		const float latitude = std::numbers::pi_v<float> * static_cast<float>(stack) / stacks;
		const float radial = std::sin(latitude);
		for (uint32_t sector = 0; sector <= sectors; ++sector)
		{
			const float u = static_cast<float>(sector) / sectors;
			const float longitude = 2.0f * std::numbers::pi_v<float> * u;
			const glm::vec3 normal{std::sin(longitude) * radial, std::cos(latitude), std::cos(longitude) * radial};
			const glm::vec3 tangent{std::cos(longitude), 0.0f, -std::sin(longitude)};
			vertices.push_back(
				{normal * 0.5f, normal, {u, static_cast<float>(stack) / stacks}, glm::vec4(tangent, 1.0f)});
		}
	}
	for (uint32_t stack = 0; stack < stacks; ++stack)
		for (uint32_t sector = 0; sector < sectors; ++sector)
		{
			const uint32_t upper = stack * (sectors + 1) + sector;
			const uint32_t lower = upper + sectors + 1;
			indices.insert(indices.end(), {upper, lower, lower + 1, upper, lower + 1, upper + 1});
		}
	return std::make_unique<TexMesh>(std::move(vertices), std::move(indices));
}

struct CaseSettings
{
	float exposure = 0.0f;
	std::string camera_pose;
	bool shadows = false;
	bool environment_lighting = false;
	std::string environment_lighting_source;
	std::string fixed_pose = "static";
	Json lights = Json::array();
};

class FidelityScene final : public DummyApplication
{
public:
	FidelityScene(WorkerOptions options, std::shared_ptr<FrameCaptureSession> capture_session) :
		options(std::move(options)), capture_session(std::move(capture_session))
	{
	}
	bool allows_playerless_normal_mode() const override { return true; }

	void on_begin(GameEngine &engine) override
	{
		engine.set_game_mode(EGameMode::NORMAL);
		engine.set_normal_mode_cursor_captured(false);
		settings = build_scene(engine);
		engine.set_exposure_ev(settings.exposure);
		engine.set_shadows_enabled(settings.shadows);
		engine.get_camera().look_at(camera_focus, camera_position);
	}

	void on_tick(GameEngine &engine, float) override
	{
		if (!engine.is_ready())
			return;
		if (!request)
		{
			request = capture_session->request_next();
			return;
		}
		if (!capture_session->try_get_result(*request))
			return;
		const auto result = capture_session->take_result(*request);
		if (!result)
			throw std::runtime_error("Capture result disappeared before consumption");
		if (result->status != FrameCaptureStatus::Success)
			throw std::runtime_error("Exact-frame capture failed: " + result->error);
		if (result->format != FrameCapturePixelFormat::RGBA8 ||
		    result->pixels.size() != size_t(result->width) * result->height * 4)
			throw std::runtime_error("Capture result violates the canonical RGBA8 contract");
		const Path image_path = options.output / (options.capture_id + ".png");
		if (!write_rgba_png(image_path, result->width, result->height, result->pixels))
			throw std::runtime_error("Unable to write capture PNG " + image_path.string());
		const auto graphics = engine.get_graphics_engine().get_runtime_graphics_info();
		if (graphics.width != 640 || graphics.height != 480 || result->width != 640 || result->height != 480)
			throw std::runtime_error("Effective presentation extent differs from the declared 640x480 capture extent");
		utsname host{};
		if (uname(&host) != 0)
			throw std::runtime_error("Unable to identify the runtime platform");
		const Json environment{
			{"vendor_id", graphics.vendor_id},
			{"device_id", graphics.device_id},
			{"device_name", graphics.device_name},
			{"driver_version", graphics.driver_version},
			{"vulkan_api_version", graphics.api_version},
			{"validation_enabled", graphics.validation_enabled},
			{"os", std::string(host.sysname) + " " + host.release},
			{"platform", host.machine},
			{"build_class", build_class},
			{"display_backend", std::getenv("KRISP_FIDELITY_VIRTUAL_DISPLAY") ? "x11-virtual" : "native"},
			{"width", graphics.width},
			{"height", graphics.height},
			{"vertex_buffer_bytes", graphics.vertex_buffer_bytes},
			{"index_buffer_bytes", graphics.index_buffer_bytes},
			{"source_format", result->source_format},
			{"source_color_space", result->source_color_space},
			{"capture_pixel_format", "RGBA8"},
			{"presentation_mode", graphics.presentation_mode}};
		Json effective{{"resolution", {result->width, result->height}},
		               {"exposure_ev", settings.exposure},
		               {"camera",
		                {{"pose", settings.camera_pose},
		                 {"position", {camera_position.x, camera_position.y, camera_position.z}},
		                 {"focus", {camera_focus.x, camera_focus.y, camera_focus.z}}}},
		               {"shadows_enabled", settings.shadows},
		               {"environment_lighting", settings.environment_lighting},
		               {"environment_lighting_source", settings.environment_lighting_source},
		               {"clear_colour", {0.0f, 0.0f, 0.0f, 1.0f}},
		               {"fixed_pose", settings.fixed_pose},
		               {"lights", settings.lights},
		               {"ui", false},
		               {"performance_overlay", false},
		               {"diagnostic_overlay", false},
		               {"interactive", false},
		               {"render_path", "rasterized-pbr-presentation"},
		               {"msaa_samples", CSTS::MSAA_SAMPLE_COUNT},
		               {"shadow_resolution", POINT_SHADOW_RESOLUTION},
		               {"vertex_buffer_bytes", graphics.vertex_buffer_bytes},
		               {"index_buffer_bytes", graphics.index_buffer_bytes},
		               {"case_definition", case_definition}};
		const auto publication = engine.get_graphics_engine().load_latest_completed_render_frames();
		if (!publication || publication->current->frame_number != result->frame_number)
			throw std::runtime_error("Captured scene snapshot is no longer the published frame");
		Json captured_renderables = Json::array();
		for (const auto &renderable : publication->current->renderables)
		{
			Json matrix = Json::array();
			for (size_t column = 0; column < 4; ++column)
				for (size_t row = 0; row < 4; ++row)
					matrix.push_back(renderable.model_transform[column][row]);
			captured_renderables.push_back({
				{"render_type", static_cast<int>(renderable.definition->pipeline_render_type)},
				{"model_transform", matrix},
				{"visible", renderable.visible},
			});
		}
		effective["captured_renderables"] = std::move(captured_renderables);
		write_json(options.output / (options.capture_id + ".worker.json"), {{"schema_version", 1},
		                                                                    {"status", "complete"},
		                                                                    {"case_id", options.case_id},
		                                                                    {"capture_id", options.capture_id},
		                                                                    {"case_version", 1},
		                                                                    {"frame_id", result->frame_number},
		                                                                    {"image",
		                                                                     {{"file", image_path.filename().string()},
		                                                                      {"width", result->width},
		                                                                      {"height", result->height},
		                                                                      {"pixel_format", "RGBA8"}}},
		                                                                    {"environment", environment},
		                                                                    {"effective_settings", effective},
		                                                                    {"fixture_files", fixture_files}});
		engine.shutdown();
	}

private:
	CaseSettings build_scene(GameEngine &engine)
	{
		auto &ecs = engine.get_ecs();
		{
			const auto definitions_path = fixture_root() / "case_definitions.json";
			std::ifstream definitions_input(definitions_path);
			if (!definitions_input)
				throw std::runtime_error("Missing case definitions: " + definitions_path.string());
			const auto definitions = Json::parse(definitions_input);
			const auto &cases = definitions.at("cases");
			if (!cases.contains(options.case_id))
				throw std::invalid_argument("Case is absent from the versioned fixture definitions: " +
				                            options.case_id);
			case_definition = cases.at(options.case_id);
			if (!case_definition.at("captures").contains(options.capture_id))
				throw std::invalid_argument("Capture ID is absent from the versioned case definition: " +
				                            options.capture_id);
			fixture_files = {
				"applications/rendering_fidelity/fixtures/case_definitions.json",
				"applications/rendering_fidelity/rendering_fidelity_worker.cpp",
			};
		}
		const auto cube = ecs.get_mesh_system().add(MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
		const auto sphere = ecs.get_mesh_system().add(make_textured_sphere());
		CaseSettings result;
		if (options.case_id == "colour-presentation")
		{
			for (const auto *file : {"base-color.png"})
				fixture_files.emplace_back("applications/rendering_fidelity/fixtures/textures/" + std::string(file));
			if (options.capture_id == "exposure_m1")
				result.exposure = -1.0f;
			else if (options.capture_id == "exposure_p1")
				result.exposure = 1.0f;
			else if (options.capture_id != "exposure_0")
				throw std::invalid_argument("Unknown colour-presentation capture ID");
			const auto base = texture(ecs, "base-color.png", ETextureSemantic::BASE_COLOR);
			const std::array<glm::vec4, 4> colours{{{1.0f, 0.25f, 0.15f, 1.0f},
			                                        {0.15f, 0.8f, 0.3f, 1.0f},
			                                        {0.2f, 0.4f, 1.0f, 1.0f},
			                                        {1.0f, 0.8f, 0.2f, 1.0f}}};
			for (size_t i = 0; i < colours.size(); ++i)
			{
				PbrMaterial::TextureSlots slots;
				slots.base_color = PbrMaterial::TextureBinding{base->get_id()};
				PbrMaterial::Properties properties;
				properties.emissive_factor = i == 3 ? glm::vec3(0.35f, 0.2f, 0.05f) : glm::vec3(0.0f);
				auto material = pbr(ecs, colours[i], 0.0f, 1.0f, slots, properties);
				add_renderable(engine, cube, material, {float(i) * 1.45f - 2.2f, 0.0f, 0.0f}, {1.1f, 1.1f, 1.1f},
				               "presentation patch", i == 3 ? EShadingMode::LIT : EShadingMode::UNLIT, {base});
			}
			add_light(engine, {0.0f, 3.0f, -1.0f}, 2400.0f, {1.0f, 0.95f, 0.85f}, false);
			result.lights = {
				{{"position", {0, 3, -1}}, {"intensity", 2400}, {"color", {1.0, 0.95, 0.85}}, {"casts_shadow", false}}};
			auto lit_material = pbr(ecs, {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 0.8f);
			add_renderable(engine, sphere, lit_material, {0.0f, 2.2f, 0.0f}, {0.65f, 0.65f, 0.65f},
			               "high intensity lit presentation patch");
			result.camera_pose = "presentation-patches";
			camera_position = {0.0f, 2.0f, -8.0f};
			camera_focus = {0.0f, 0.0f, 0.0f};
		}
		else if (options.case_id == "pbr-factors")
		{
			if (options.capture_id != "grid")
				throw std::invalid_argument("Unknown pbr-factors capture ID");
			add_light(engine, {-3.0f, 5.0f, -4.0f}, 700.0f, {1.0f, 0.92f, 0.8f}, false);
			for (int row = 0; row < 4; ++row)
				for (int col = 0; col < 5; ++col)
				{
					auto material = pbr(ecs, {0.7f, 0.23f, 0.08f, 1.0f}, float(row) / 3.0f, 0.12f + 0.2f * col);
					add_renderable(engine, sphere, material, {float(col) - 2.0f, 1.6f - float(row) * 1.05f, 0.0f},
					               {0.42f, 0.42f, 0.42f}, "metallic roughness sphere");
				}
			result.camera_pose = "sphere-factor-grid";
			result.lights = {
				{{"position", {-3, 5, -4}}, {"intensity", 700}, {"color", {1.0, 0.92, 0.8}}, {"casts_shadow", false}}};
			camera_position = {0.0f, 0.5f, -10.0f};
			camera_focus = {0.0f, 0.0f, 0.0f};
		}
		else if (options.case_id == "textured-normals")
		{
			for (const auto *file : {"base-color.png", "metallic-roughness.png", "normal.png", "emissive.png"})
				fixture_files.emplace_back("applications/rendering_fidelity/fixtures/textures/" + std::string(file));
			if (options.capture_id != "all_maps")
				throw std::invalid_argument("Unknown textured-normals capture ID");
			auto base = texture(ecs, "base-color.png", ETextureSemantic::BASE_COLOR);
			auto mr = texture(ecs, "metallic-roughness.png", ETextureSemantic::METALLIC_ROUGHNESS);
			auto normal = texture(ecs, "normal.png", ETextureSemantic::NORMAL);
			auto emissive = texture(ecs, "emissive.png", ETextureSemantic::EMISSIVE);
			PbrMaterial::TextureSlots slots;
			slots.base_color = PbrMaterial::TextureBinding{base->get_id()};
			slots.metallic_roughness = PbrMaterial::TextureBinding{mr->get_id()};
			slots.normal = PbrMaterial::TextureBinding{normal->get_id()};
			slots.emissive = PbrMaterial::TextureBinding{emissive->get_id()};
			PbrMaterial::Properties properties;
			properties.emissive_factor = {0.25f, 0.12f, 0.04f};
			auto material = pbr(ecs, glm::vec4(1.0f), 1.0f, 1.0f, slots, properties);
			add_renderable(engine, sphere, material, {0, 0, 0}, {2.0f, 2.0f, 2.0f}, "all PBR texture maps",
			               EShadingMode::LIT, {base, mr, normal, emissive});
			add_light(engine, {-2.0f, 4.0f, -4.0f}, 900.0f, {1.0f, 0.9f, 0.75f}, false);
			result.camera_pose = "textured-sphere";
			result.lights = {
				{{"position", {-2, 4, -4}}, {"intensity", 900}, {"color", {1.0, 0.9, 0.75}}, {"casts_shadow", false}}};
			camera_position = {0.0f, 0.0f, -7.0f};
			camera_focus = {0, 0, 0};
		}
		else if (options.case_id == "environment-lighting")
		{
			if (options.capture_id != "ibl_roughness")
				throw std::invalid_argument("Unknown environment-lighting capture ID");
			result.environment_lighting = true;
			result.environment_lighting_source = ResourcePaths::get_engine_paths().environment_lighting.string();
			fixture_files.push_back(result.environment_lighting_source);
			engine.spawn_cubemap();
			add_light(engine, {-4, 5, -4}, 300.0f, {1, 1, 1}, false);
			for (int i = 0; i < 7; ++i)
			{
				auto material = pbr(ecs, {0.8f, 0.72f, 0.55f, 1.0f}, 1.0f, float(i) / 6.0f);
				add_renderable(engine, sphere, material, {float(i) - 3.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f},
				               "environment roughness sphere");
			}
			result.camera_pose = "environment-roughness-row";
			result.lights = {
				{{"position", {-4, 5, -4}}, {"intensity", 300}, {"color", {1, 1, 1}}, {"casts_shadow", false}}};
			camera_position = {0.0f, 1.0f, -9.0f};
			camera_focus = {0, 0, 0};
		}
		else if (options.case_id == "point-shadows")
		{
			if (options.capture_id != "shadowed" && options.capture_id != "unshadowed")
				throw std::invalid_argument("Unknown point-shadows capture ID");
			result.shadows = options.capture_id == "shadowed";
			const auto floor = ecs.get_mesh_system().add(MeshFactory::cube());
			const auto receiver = pbr(ecs, {0.62f, 0.62f, 0.62f, 1.0f}, 0.0f, 0.9f);
			const auto red = pbr(ecs, {0.8f, 0.12f, 0.08f, 1.0f}, 0.0f, 0.5f);
			const auto blue = pbr(ecs, {0.08f, 0.25f, 0.8f, 1.0f}, 0.0f, 0.5f);
			add_renderable(engine, floor, receiver, {0, -1.2f, 0}, {6.0f, 0.15f, 5.0f}, "shadow receiver");
			add_renderable(engine, cube, red, {-1.2f, 0.0f, 0.2f}, {1.0f, 1.2f, 1.0f}, "shadow caster");
			add_renderable(engine, cube, blue, {1.3f, -0.1f, 0.0f}, {0.9f, 0.9f, 0.9f}, "shadow caster");
			const std::array<std::pair<glm::vec3, glm::vec3>, 3> lights{{{{-3, 4, -3}, {1.0f, 0.15f, 0.1f}},
			                                                             {{3, 4, -2}, {0.1f, 0.25f, 1.0f}},
			                                                             {{0, 3, 3}, {0.1f, 1.0f, 0.25f}}}};
			for (const auto &[position, colour] : lights)
			{
				add_light(engine, position, 500.0f, colour, true);
				result.lights.push_back({{"position", {position.x, position.y, position.z}},
				                         {"intensity", 500.0f},
				                         {"color", {colour.x, colour.y, colour.z}},
				                         {"casts_shadow", true}});
			}
			result.camera_pose = "caster-receiver-multiple-lights";
			camera_position = {0.0f, 2.4f, -9.0f};
			camera_focus = {0, -0.5f, 0};
		}
		else if (options.case_id == "visibility-alpha")
		{
			if (options.capture_id != "depth_sidedness_alpha")
				throw std::invalid_argument("Unknown visibility-alpha capture ID");
			const auto opaque = pbr(ecs, {0.15f, 0.35f, 0.8f, 1}, 0, 0.7f);
			fixture_files.emplace_back("applications/rendering_fidelity/fixtures/textures/alpha-mask.png");
			auto alpha_texture = texture(ecs, "alpha-mask.png", ETextureSemantic::BASE_COLOR);
			PbrMaterial::TextureSlots mask_slots;
			mask_slots.base_color = PbrMaterial::TextureBinding{alpha_texture->get_id()};
			PbrMaterial::Properties mask;
			mask.alpha_mode = EAlphaMode::MASK;
			mask.alpha_cutoff = 0.5f;
			mask.double_sided = true;
			PbrMaterial::Properties blend;
			blend.alpha_mode = EAlphaMode::BLEND;
			blend.double_sided = true;
			const auto masked = pbr(ecs, {1, 1, 1, 1}, 0, 0.6f, mask_slots, mask);
			const auto blended = pbr(ecs, {0.1f, 0.8f, 0.55f, 0.4f}, 0, 0.3f, {}, blend);
			const auto one_sided = pbr(ecs, {0.85f, 0.2f, 0.15f, 1}, 0, 0.6f);
			add_renderable(engine, cube, opaque, {-1.1f, 0, 0}, {2, 2, 2}, "depth front object");
			const auto quad = ecs.get_mesh_system().add(MeshFactory::quad(MeshFactory::EVertexType::TEXTURE));
			add_renderable(engine, quad, masked, {-1.5f, 0.0f, 0.8f}, {2.5f, 2.5f, 1.0f}, "alpha masked open surface",
			               EShadingMode::LIT, {alpha_texture});
			add_renderable(engine, quad, one_sided, {1.5f, 0.0f, 0.8f}, {2.5f, 2.5f, 1.0f}, "single-sided back face");
			auto &facing = engine.spawn_object<Object>();
			ecs.get_transformation(facing.get_id()).set_position({1.5f, 0.0f, 0.7f});
			ecs.get_transformation(facing.get_id()).set_rotation(glm::angleAxis(Maths::PI, glm::vec3(0, 1, 0)));
			engine.attach_renderable(facing.get_id(), Renderable{.name = "opposite-facing single-sided open surface",
			                                                     .pipeline_render_type = ERenderType::STANDARD,
			                                                     .mesh_owner = quad,
			                                                     .material_owners = {one_sided}});
			add_renderable(engine, quad, blended, {0.0f, 0.0f, 1.0f}, {2.5f, 2.5f, 1.0f}, "first blended surface");
			add_renderable(engine, quad, pbr(ecs, {0.85f, 0.18f, 0.72f, 0.35f}, 0, 0.35f, {}, blend),
			               {0.0f, 0.0f, 0.5f}, {2.1f, 2.1f, 1.0f}, "second blended surface");
			add_light(engine, {-3, 4, -4}, 700, {1, 1, 1}, false);
			result.camera_pose = "overlap-depth-front-and-back-alpha";
			result.lights = {
				{{"position", {-3, 4, -4}}, {"intensity", 700}, {"color", {1, 1, 1}}, {"casts_shadow", false}}};
			camera_position = {0, 1.2f, -8};
			camera_focus = {0, 0, 0};
		}
		else if (options.case_id == "hierarchy-skinning")
		{
			if (options.capture_id != "bind_pose" && options.capture_id != "animated_pose")
				throw std::invalid_argument("Unknown hierarchy-skinning capture ID");
			result.fixed_pose = options.capture_id;
			const auto material = pbr(ecs, {0.8f, 0.35f, 0.12f, 1}, 0.0f, 0.55f);
			auto &parent = engine.spawn_object<Object>();
			ecs.get_transformation(parent.get_id()).set_position({-1.2f, 0, 0});
			auto &child = engine.spawn_object<Object>();
			auto &child_transform = ecs.get_transformation(child.get_id());
			child_transform.attach_to(parent.get_id());
			child_transform.set_relative_position({0.0f, 0.8f, 0.0f});
			engine.attach_renderable(child.get_id(), Renderable{.name = "nested hierarchy mesh",
			                                                    .pipeline_render_type = ERenderType::STANDARD,
			                                                    .mesh_owner = sphere,
			                                                    .material_owners = {material}});
			const std::vector<Bone> bones{
				Bone{.name = "root"},
				Bone{.relative_transform = Maths::Transform({0, 0.6f, 0}, {1, 1, 1}, Maths::identity_quat),
			         .inverse_bind_pose = Maths::Transform({0, -0.6f, 0}, {1, 1, 1}, Maths::identity_quat),
			         .name = "tip",
			         .parent_node = 0}};
			const auto skeleton = ecs.add_skeleton(bones);
			std::vector<SDS::SkinnedVertex> vertices{
				{{0, 0, 0, 0}, {1, 0, 0, 0}, {-0.6f, -1.0f, 0}, {0, 0, -1}, {0, 0}, {1, 0, 0, 1}},
				{{0, 0, 0, 0}, {1, 0, 0, 0}, {0.6f, -1.0f, 0}, {0, 0, -1}, {1, 0}, {1, 0, 0, 1}},
				{{1, 0, 0, 0}, {1, 0, 0, 0}, {-0.6f, 0, 0}, {0, 0, -1}, {0, 1}, {1, 0, 0, 1}},
				{{1, 0, 0, 0}, {1, 0, 0, 0}, {0.6f, 0, 0}, {0, 0, -1}, {1, 1}, {1, 0, 0, 1}},
				{{1, 0, 0, 0}, {1, 0, 0, 0}, {-0.6f, 1.0f, 0}, {0, 0, -1}, {0, 2}, {1, 0, 0, 1}},
				{{1, 0, 0, 0}, {1, 0, 0, 0}, {0.6f, 1.0f, 0}, {0, 0, -1}, {1, 2}, {1, 0, 0, 1}}};
			const auto skinned = ecs.get_mesh_system().add(std::make_unique<SkinnedMesh>(
				std::move(vertices), std::vector<uint32_t>{0, 2, 1, 2, 3, 1, 2, 4, 3, 4, 5, 3}));
			auto &skinned_object = engine.spawn_object<Object>();
			ecs.get_transformation(skinned_object.get_id()).set_position({1.4f, -0.1f, 0});
			engine.attach_renderable(skinned_object.get_id(),
			                         Renderable{.name = "two-bone skinned strip",
			                                    .pipeline_render_type = ERenderType::SKINNED_COLOR,
			                                    .mesh_owner = skinned,
			                                    .material_owners = {material}},
			                         skeleton);
			if (options.capture_id == "animated_pose")
				ecs.get_skeletal_component(skeleton).get_bone_local_transform(1).set_orient(
					glm::angleAxis(0.65f, glm::vec3(0, 0, 1)));
			add_light(engine, {-3, 4, -4}, 600, {1, 1, 1}, false);
			result.camera_pose = "nested-static-and-two-bone-skin";
			result.lights = {
				{{"position", {-3, 4, -4}}, {"intensity", 600}, {"color", {1, 1, 1}}, {"casts_shadow", false}}};
			camera_position = {0, 0.5f, -7};
			camera_focus = {0, 0.2f, 0};
		}
		else if (options.case_id == "textured-environment")
		{
			if (options.capture_id != "sponza_atrium")
				throw std::invalid_argument("Unknown textured-environment capture ID");
			if (options.asset_cache.empty())
				throw std::runtime_error(
					"textured-environment requires --asset-cache; prepare the pinned Sponza assets first");
			const auto prepared_path = options.asset_cache / "prepared.json";
			std::ifstream input(prepared_path);
			if (!input)
				throw std::runtime_error("Missing verified prepared asset manifest: " + prepared_path.string());
			const auto prepared = Json::parse(input);
			if (prepared.value("profile", std::string{}) != "engine-pbr-opaque-bc3-normal-mips-v1")
				throw std::runtime_error("Prepared Sponza profile does not match the pinned fidelity profile");
			fixture_files.push_back(prepared_path.string());
			for (auto it = prepared.at("files").begin(); it != prepared.at("files").end(); ++it)
				fixture_files.push_back((options.asset_cache / it.key()).string());
			const auto asset = prepared.at("assets").at("sponza");
			const auto gltf_relative = asset.at("gltf").get<std::string>();
			const auto registered = engine.assets().load_asset(gltf_relative);
			const auto model = engine.assets().instantiate_model(ecs, AssetRegistry::default_assembly(registered));
			if (!model.warnings.empty())
				throw std::runtime_error("Prepared Sponza produced unexpected importer warnings");
			auto &object = engine.spawn_object<Object>();
			ecs.get_transformation(object.get_id()).set_transform(model.onload_transform.get_mat4());
			for (const auto &part : model.meshes)
				for (const auto &renderable : part.renderables)
					engine.attach_renderable(object.get_id(), renderable);
			const auto &view = asset.at("views").at("atrium");
			camera_focus = vector3(view.at("focus"));
			camera_position = vector3(view.at("from"));
			const glm::vec3 light_position = vector3(asset.at("light_position"));
			add_light(engine, light_position, asset.at("light_intensity").get<float>() / 3.0f, {1, 1, 1}, true);
			const glm::vec3 second_position = light_position + glm::vec3(-8, -4, -6);
			const glm::vec3 third_position = light_position + glm::vec3(8, -4, 6);
			const float light_intensity = asset.at("light_intensity").get<float>() / 3.0f;
			add_light(engine, second_position, light_intensity, {0.8f, 0.9f, 1.0f}, true);
			add_light(engine, third_position, light_intensity, {1.0f, 0.85f, 0.7f}, true);
			result.shadows = true;
			result.camera_pose = "prepared-sponza-atrium";
			result.lights = {{{"position", {light_position.x, light_position.y, light_position.z}},
			                  {"intensity", light_intensity},
			                  {"color", {1, 1, 1}},
			                  {"casts_shadow", true}},
			                 {{"position", {second_position.x, second_position.y, second_position.z}},
			                  {"intensity", light_intensity},
			                  {"color", {0.8, 0.9, 1.0}},
			                  {"casts_shadow", true}},
			                 {{"position", {third_position.x, third_position.y, third_position.z}},
			                  {"intensity", light_intensity},
			                  {"color", {1.0, 0.85, 0.7}},
			                  {"casts_shadow", true}}};
		}
		else
			throw std::invalid_argument("Unknown fidelity case " + options.case_id);
		return result;
	}

	WorkerOptions options;
	std::shared_ptr<FrameCaptureSession> capture_session;
	CaseSettings settings;
	Json case_definition;
	std::vector<std::string> fixture_files;
	std::optional<FrameCaptureSession::RequestId> request;
	glm::vec3 camera_position{0.0f, 1.0f, -8.0f};
	glm::vec3 camera_focus{0.0f};
};

Json error_manifest(const WorkerOptions &options, const std::string &message)
{
	return {{"schema_version", 1},
	        {"status", "error"},
	        {"case_id", options.case_id},
	        {"capture_id", options.capture_id},
	        {"error", message}};
}
} // namespace

int run_rendering_fidelity_worker(int argc, char **argv)
{
	WorkerOptions options;
	try
	{
		options = parse_options(argc, argv);
		if (options.case_id == "textured-environment" && options.asset_cache.empty())
			throw std::runtime_error(
				"textured-environment requires --asset-cache; prepare the pinned Sponza assets first");
		std::filesystem::create_directories(options.output);
		auto paths = ResourcePaths::paths_for_executable(PROJECT_NAME);
		paths.app_resources = options.case_id == "textured-environment" ? options.asset_cache : fixture_root();
		Config::init(PROJECT_NAME, std::move(paths));
		RuntimeOptions runtime;
		runtime.width = 640;
		runtime.height = 480;
		runtime.simulation_hz = 60.0;
		runtime.fixed_simulation_timestep = true;
		runtime.vertex_buffer_bytes = vertex_buffer_capacity;
		runtime.index_buffer_bytes = index_buffer_capacity;
		runtime.interactive = false;
		runtime.ui = false;
		runtime.performance_overlay = false;
		runtime.frame_capture = std::make_shared<FrameCaptureSession>();
		auto session = runtime.frame_capture;
		auto engine = GameEngine::create_with_options<FidelityScene>(runtime, options, session);
		engine.run();
		if (const auto failure = engine.get_run_failure())
			std::rethrow_exception(failure);
		const auto manifest = options.output / (options.capture_id + ".worker.json");
		if (!std::filesystem::is_regular_file(manifest))
			throw std::runtime_error("Worker ended without writing capture metadata");
		return 0;
	} catch (const std::exception &error)
	{
		if (!options.output.empty() && !options.case_id.empty() && !options.capture_id.empty())
		{
			try
			{
				std::filesystem::create_directories(options.output);
				write_json(options.output / (options.capture_id + ".worker.json"),
				           error_manifest(options, error.what()));
			} catch (...)
			{
			}
		}
		std::cerr << error.what() << '\n';
		return 1;
	}
}
