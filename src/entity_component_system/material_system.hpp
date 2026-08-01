#pragma once

#include "countable_system.hpp"
#include "renderable/material.hpp"


// Materials are mutable while constructed and become immutable when registered.
using MaterialSystem = CountableSystem<MaterialID, const Material>;
using MaterialHandle = MaterialSystem::HandlePtr;
