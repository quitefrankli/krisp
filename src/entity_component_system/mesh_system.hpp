#pragma once

#include "countable_system.hpp"
#include "renderable/mesh.hpp"


// Meshes are mutable while constructed and become immutable when registered.
using MeshSystem = CountableSystem<MeshID, const Mesh>;
using MeshHandle = MeshSystem::HandlePtr;
