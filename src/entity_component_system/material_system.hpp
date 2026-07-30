#pragma once

#include "common.hpp"
#include "renderable/material.hpp"


using MaterialSystem = CountableSystem<MaterialID, Material>;
using MaterialHandle = MaterialSystem::HandlePtr;
