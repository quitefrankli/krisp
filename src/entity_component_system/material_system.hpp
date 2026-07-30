#pragma once

#include "countable_system.hpp"
#include "renderable/material.hpp"


using MaterialSystem = CountableSystem<MaterialID, Material>;
using MaterialHandle = MaterialSystem::HandlePtr;
