#pragma once

#include "objects/objects.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct SharedLibFuncPtrs
{
	void (*load_check)();
};