#include "../rendering_fidelity/rendering_fidelity_worker.hpp"
#include "performance/benchmark_worker.hpp"

#include <resource_paths.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace
{
void print_usage(std::ostream& output)
{
	output << "Usage: benchmarks <performance|fidelity> <command> [options...]\n"
	       << "  performance  Run performance benchmark suites\n"
	       << "  fidelity     Run rendering fidelity checks\n";
}
}

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		const std::string_view mode(argv[1]);
		if (mode == "_worker")
			return run_benchmark_worker(argc, argv);
		if (mode == "_fidelity-worker")
			return run_rendering_fidelity_worker(argc, argv);
	}

	if (argc < 2 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")
	{
		print_usage(std::cout);
		return 0;
	}

	const std::string_view mode(argv[1]);
	if (mode != "performance" && mode != "fidelity")
	{
		print_usage(std::cerr);
		return 2;
	}
#if !defined(NDEBUG) || defined(DEBUG) || !defined(__OPTIMIZE__)
	if (mode == "performance")
	{
		std::cerr << "Performance benchmarks require an optimized Release build with assertions disabled.\n";
		return 2;
	}
#endif

	try
	{
		const auto executable = std::filesystem::read_symlink("/proc/self/exe");
		const auto directory = executable.parent_path();
		const bool performance = mode == "performance";
		const bool prepare_assets = performance && argc > 2 && std::string_view(argv[2]) == "prepare-assets";
		const auto tool = performance
			? (prepare_assets ? "benchmark_assets.py" : "benchmark_runner.py")
			: "fidelity_runner.py";
		auto script = directory / "benchmarks-tools" / (performance ? "performance" : "fidelity") / tool;
		if (!std::filesystem::exists(script))
			script = std::filesystem::path(KRISP_SOURCE_DIR) /
				(performance ? "applications/benchmarks/performance/" : "applications/rendering_fidelity/") / tool;

		std::vector<std::string> arguments{KRISP_PYTHON, script.string()};
		if (performance)
		{
			const auto asset_cache = std::filesystem::path(KRISP_SOURCE_DIR) / "build" / "benchmark-assets";
			arguments.insert(arguments.end(), {"--default-asset-cache", asset_cache.string()});
			if (!prepare_assets)
			{
				const auto paths = ResourcePaths::paths_for_executable("benchmarks").engine;
				arguments.insert(arguments.end(), {"--executable", executable.string(), "--build-manifest",
					(directory / "benchmarks-build.json").string(), "--environment-lighting",
					paths.environment_lighting.string(), "--shader-directory", paths.shaders.string()});
			}
		}
		else
		{
			arguments.insert(arguments.end(), {"--executable", executable.string(), "--source-root", KRISP_SOURCE_DIR});
		}
		for (int index = prepare_assets ? 3 : 2; index < argc; ++index)
			arguments.emplace_back(argv[index]);

		std::vector<char*> pointers;
		for (auto& argument : arguments)
			pointers.push_back(argument.data());
		pointers.push_back(nullptr);
		execv(KRISP_PYTHON, pointers.data());
		throw std::runtime_error("Unable to launch benchmark tools using " + std::string(KRISP_PYTHON));
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
