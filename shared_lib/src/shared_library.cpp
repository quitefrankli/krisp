#include "shared_library.hpp"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>


void SharedLib::foo()
{
	glm::vec3 vec;
	
	std::cout << glm::to_string(vec) << std::endl;
}