#include "rendering_fidelity_worker.hpp"

#include <resource_paths.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc > 1 && std::string_view(argv[1]) == "_worker")
		return run_rendering_fidelity_worker(argc, argv);
	try
	{
		const auto executable = std::filesystem::read_symlink("/proc/self/exe");
		const auto directory = executable.parent_path();
		auto script = directory / "rendering_fidelity-tools" / "fidelity_runner.py";
		if (!std::filesystem::exists(script))
			script = std::filesystem::path(KRISP_SOURCE_DIR) / "applications/rendering_fidelity/fidelity_runner.py";
		std::vector<std::string> arguments{KRISP_PYTHON, script.string()};
		arguments.emplace_back("--executable");
		arguments.push_back(executable.string());
		arguments.emplace_back("--source-root");
		arguments.emplace_back(KRISP_SOURCE_DIR);
		for (int index = 1; index < argc; ++index)
			arguments.emplace_back(argv[index]);
		std::vector<char *> pointers;
		for (auto &argument : arguments)
			pointers.push_back(argument.data());
		pointers.push_back(nullptr);
		execv(KRISP_PYTHON, pointers.data());
		throw std::runtime_error("Unable to launch rendering_fidelity tools using " + std::string(KRISP_PYTHON));
	} catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
