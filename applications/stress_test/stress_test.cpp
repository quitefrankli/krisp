#include "benchmark_worker.hpp"
#include <resource_paths.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

int main(int argc, char** argv)
{
	if (argc > 1 && std::string_view(argv[1]) == "_worker")
		return run_benchmark_worker(argc, argv);

	try
	{
		const auto executable = std::filesystem::read_symlink("/proc/self/exe");
		const auto directory = executable.parent_path();
		const bool prepare = argc > 1 && std::string_view(argv[1]) == "prepare-assets";
		const auto filename = prepare ? "benchmark_assets.py" : "benchmark_runner.py";
		auto script = directory / "stress_test-tools" / filename;
		if (!std::filesystem::exists(script))
			script = std::filesystem::path(KRISP_SOURCE_DIR) / "applications/stress_test" / filename;
		std::vector<std::string> arguments{KRISP_PYTHON, script.string()};
		const auto asset_cache = std::filesystem::path(KRISP_SOURCE_DIR) / "build" / "benchmark-assets";
		if (prepare)
			arguments.insert(arguments.end(), {"--default-asset-cache", asset_cache.string()});
		if (!prepare)
		{
			const auto paths = ResourcePaths::paths_for_executable("stress_test").engine;
			arguments.insert(arguments.end(), {"--default-asset-cache", asset_cache.string(), "--executable", executable.string(), "--build-manifest",
				(directory / "stress_test-build.json").string(), "--environment-lighting",
				paths.environment_lighting.string(), "--shader-directory", paths.shaders.string()});
		}
		for (int i = prepare ? 2 : 1; i < argc; ++i) arguments.emplace_back(argv[i]);
		std::vector<char*> pointers;
		for (auto& argument : arguments) pointers.push_back(argument.data());
		pointers.push_back(nullptr);
		execv(KRISP_PYTHON, pointers.data());
		throw std::runtime_error("Unable to launch stress_test tools using " + std::string(KRISP_PYTHON));
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
