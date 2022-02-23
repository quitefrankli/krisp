#pragma once

#include "objects/objects.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class Arrow;

struct SharedLibFuncPtrs
{
	void (*load_check)();
	void (*point_arrow)(Arrow&);
};