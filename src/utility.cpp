#include "utility.hpp"

#include <quill/Quill.h>

#include <iostream>


Utility Utility::singleton;

Utility::Utility()
{
	top_level_dir = PROJECT_TOP_LEVEL_SRC_DIR;
	models = get_child(top_level_dir, "resources/models");
	textures = get_child(top_level_dir, "resources/textures");
	audio = get_child(top_level_dir, "resources/sound");
	
	build = PROJECT_BUILD_DIR;
	binary = PROJECT_BIN_DIR;
	shaders = get_child(build, "shaders");

	auto file_handler = quill::file_handler("log.log", "a");
	logger = quill::create_logger("MAIN", file_handler);
	file_handler->set_pattern(
		QUILL_STRING("%(ascii_time): %(message)"),
		"%D %H:%M:%S.%Qus",
		quill::Timezone::LocalTime
	);

#ifdef PROJECT_ENABLE_LOGGING
	quill::start(); // this will consume CPU cycles
#endif

	std::cout << models << '\n' << textures << '\n' << build << '\n' << binary << '\n';
}

std::filesystem::path Utility::get_child(const std::filesystem::path& parent, const std::string_view child)
{
	return std::filesystem::path(parent.string() + '/' + child.data());
}
