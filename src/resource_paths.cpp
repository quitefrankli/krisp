#include "resource_paths.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <system_error>

#include "krisp_engine_paths.hpp"

namespace
{
	std::unique_ptr<ResourcePaths> resource_paths_singleton;

	std::filesystem::path xdg_directory(const char* variable, const char* home_suffix, std::string_view app_name)
	{
		if (const char* configured = std::getenv(variable); configured && *configured)
			return std::filesystem::path(configured) / app_name;
		if (const char* home = std::getenv("HOME"); home && *home)
			return std::filesystem::path(home) / home_suffix / app_name;
		return std::filesystem::temp_directory_path() / app_name;
	}

	void validate_relative_filename(std::string_view filename, std::string_view operation)
	{
		const std::filesystem::path relative(filename);
		if (relative.empty() || relative.is_absolute()
			|| std::ranges::find(relative, std::filesystem::path("..")) != relative.end())
		{
			throw std::runtime_error(fmt::format(
				"{}: expected a resource-relative filename: '{}'", operation, filename));
		}
	}
}

RuntimePaths ResourcePaths::paths_for_executable(std::string_view app_name)
{
	std::error_code error;
	auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
	if (error)
		throw std::runtime_error(fmt::format("ResourcePaths::paths_for_executable: cannot resolve executable: {}", error.message()));
	const auto executable_dir = executable.parent_path();
	EnginePaths engine{
		.resources = KRISP_ENGINE_RESOURCES,
		.shaders = KRISP_ENGINE_SHADERS,
		.default_config = KRISP_ENGINE_DEFAULT_CONFIG,
		.environment_lighting = KRISP_ENGINE_ENVIRONMENT_LIGHTING,
	};
	if (engine.resources.empty())
	{
		const auto runtime_dir = executable_dir / "krisp-runtime";
		engine = EnginePaths{
			.resources = runtime_dir / "resources",
			.shaders = runtime_dir / "shaders",
			.default_config = runtime_dir / "configs" / "default.yaml",
			.environment_lighting = runtime_dir / "default_environment.krisp-ibl",
		};
	}
	return RuntimePaths{
		.app_resources = executable_dir / "resources" / app_name,
		.app_config = xdg_directory("XDG_CONFIG_HOME", ".config", app_name),
		.writable_data = xdg_directory("XDG_DATA_HOME", ".local/share", fmt::format("krisp/{}", app_name)),
		.engine = std::move(engine),
	};
}

void ResourcePaths::initialize(RuntimePaths paths)
{
	if (resource_paths_singleton)
		throw std::runtime_error("ResourcePaths::initialize: runtime paths are already configured");
	if (paths.app_resources.empty() || paths.app_config.empty() || paths.writable_data.empty()
		|| paths.engine.resources.empty() || paths.engine.shaders.empty()
		|| paths.engine.default_config.empty() || paths.engine.environment_lighting.empty())
		throw std::invalid_argument("ResourcePaths::initialize: all runtime paths must be provided");
	resource_paths_singleton.reset(new ResourcePaths(std::move(paths)));
}

ResourcePaths::ResourcePaths(RuntimePaths runtime_paths) : paths(std::move(runtime_paths)) {}

ResourcePaths& ResourcePaths::get()
{
	if (!resource_paths_singleton)
		throw std::runtime_error("ResourcePaths used before Config::init configured runtime paths");
	return *resource_paths_singleton;
}

const EnginePaths& ResourcePaths::get_engine_paths()
{
	return get().paths.engine;
}

const std::filesystem::path& ResourcePaths::get_writable_data_path()
{
	return get().paths.writable_data;
}

const std::filesystem::path& ResourcePaths::get_test_data_path()
{
	return get().test_data;
}

std::filesystem::path ResourcePaths::get_config_path(std::string_view filename)
{
	validate_relative_filename(filename, "ResourcePaths::get_config_path");
	auto app_config = get().paths.app_config / filename;
	if (std::filesystem::exists(app_config))
		return app_config;
	if (filename == "default.yaml")
		return get().paths.engine.default_config;
	return get().paths.engine.default_config.parent_path() / filename;
}

std::filesystem::path ResourcePaths::get_user_config_path(std::string_view filename)
{
	validate_relative_filename(filename, "ResourcePaths::get_user_config_path");
	return get().paths.app_config / filename;
}

std::filesystem::path ResourcePaths::get_rsrc_path(bool use_default)
{
	return use_default ? get().paths.engine.resources : get().paths.app_resources;
}

std::filesystem::path ResourcePaths::resolve_resource(std::string_view subdir, std::string_view filename)
{
	validate_relative_filename(filename, "ResourcePaths::resolve_resource");
	const std::filesystem::path resource_name(filename);

	if (get().test_mode)
	{
		auto test_path = get().test_data / resource_name;
		if (std::filesystem::exists(test_path))
			return test_path;
	}

	auto app_path = get_rsrc_path() / subdir / resource_name;
	if (std::filesystem::exists(app_path))
		return app_path;

	auto default_path = get_rsrc_path(true) / subdir / resource_name;
	if (std::filesystem::exists(default_path))
		return default_path;

	throw std::runtime_error(fmt::format(
		"ResourcePaths::resolve_resource: resource not found in app/default paths. subdir='{}', filename='{}'",
		subdir,
		filename));
}

std::filesystem::path ResourcePaths::get_texture(std::string_view filename)
{
	return resolve_resource("textures", filename);
}

std::filesystem::path ResourcePaths::get_resource(std::string_view filename)
{
	const std::filesystem::path name(filename);
	if (get().test_mode && name.has_parent_path())
	{
		validate_relative_filename(filename, "ResourcePaths::get_resource");
		auto component = name.begin();
		if (*component == "meshes" || *component == "textures" || *component == "animations" || *component == "kassets")
		{
			std::filesystem::path fixture;
			for (++component; component != name.end(); ++component) fixture /= *component;
			const auto fixture_path = get().test_data / fixture;
			if (std::filesystem::is_regular_file(fixture_path)) return fixture_path;
		}
	}
	if (name.has_parent_path())
	{
		const auto category = *name.begin();
		if (category == "meshes" || category == "textures" || category == "animations" || category == "kassets")
			return resolve_resource("", filename);
	}
	const auto extension = name.extension();
	if (extension == ".kasset") return resolve_resource("kassets", filename);
	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds")
		return resolve_resource("textures", filename);
	return resolve_resource("meshes", filename);
}

std::filesystem::path ResourcePaths::get_model(std::string_view filename)
{
	return resolve_resource("meshes", filename);
}

std::filesystem::path ResourcePaths::get_animation(std::string_view filename)
{
	return resolve_resource("animations", filename);
}

std::filesystem::path ResourcePaths::get_shader(std::string_view filename)
{
	validate_relative_filename(filename, "ResourcePaths::get_shader");
	auto app_path = get_rsrc_path() / "shaders" / filename;
	if (std::filesystem::exists(app_path))
		return app_path;
	return get().paths.engine.shaders / filename;
}

void ResourcePaths::set_test_mode(std::filesystem::path test_data)
{
	if (test_data.empty() || !test_data.is_absolute())
		throw std::invalid_argument("ResourcePaths::set_test_mode: test data path must be absolute");
	get().test_data = std::move(test_data);
	get().test_mode = true;
}

std::filesystem::path ResourcePaths::get_audio(std::string_view filename)
{
	return resolve_resource("sound", filename);
}

std::vector<std::string> ResourcePaths::collect_resources(
	std::string_view subdir,
	const std::unordered_set<std::string_view>& extensions)
{
	std::unordered_set<std::string> seen_filenames;
	std::vector<std::string> files;

	auto collect_from = [&](const std::filesystem::path& dir) {
		if (!std::filesystem::exists(dir)) return;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
		{
			if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
			{
				auto filename = entry.path().lexically_relative(dir).generic_string();
				if (seen_filenames.insert(filename).second)
					files.push_back(std::move(filename));
			}
		}
	};

	collect_from(get_rsrc_path() / subdir);
	collect_from(get_rsrc_path(true) / subdir);
	return files;
}

std::vector<std::string> ResourcePaths::get_all_textures()
{
	return collect_resources("textures", { ".jpg", ".jpeg", ".png", ".bmp", ".tga", ".dds" });
}

std::vector<std::string> ResourcePaths::get_all_models()
{
	return collect_resources("meshes", { ".gltf", ".glb", ".obj", ".fbx" });
}

std::vector<std::string> ResourcePaths::get_all_animations()
{
	return collect_resources("animations", { ".gltf", ".glb" });
}

std::vector<std::string> ResourcePaths::get_all_audio()
{
	return collect_resources("sound", { ".wav", ".ogg", ".mp3", ".flac" });
}
