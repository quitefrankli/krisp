#pragma once

#include <glm/glm.hpp>
#include <type_traits>

class HotReload
{
public:
	// reloads dll
	static void reload();


	using func1_t = bool (*)(glm::mat4&, glm::mat4, glm::mat4, glm::vec2);
	static func1_t func1;
};