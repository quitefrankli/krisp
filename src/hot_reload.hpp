#pragma once

#include <glm/glm.hpp>
#include <type_traits>


class Arrow;

class HotReload
{
public:
	// reloads dll
	static void reload();

	using func1_t = bool (*)(glm::mat4&, glm::mat4, glm::mat4, glm::vec2);
	static func1_t func1;

	using func2_t = bool (*)(const glm::vec3&, const glm::vec3&, const glm::vec3&, float);
	static func2_t func2;

	using func3_t = void (*)(Arrow&);
	static func3_t func3;
};