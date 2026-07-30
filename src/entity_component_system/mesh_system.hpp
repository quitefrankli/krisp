#pragma once

#include "countable_system.hpp"
#include "renderable/mesh.hpp"


using MeshSystem = CountableSystem<MeshID, Mesh>;
using MeshHandle = MeshSystem::HandlePtr;
