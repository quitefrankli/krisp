#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>


namespace BenchmarkScene
{
inline constexpr int lattice_cell_count = 32 * 32;
inline constexpr int maximum_cube_count = 1000;

// Returns the center of one cell in the stable lattice prefix. Indices must be
// in [0, lattice_cell_count); every valid index maps to a unique cell.
inline glm::vec3 cube_position(int index)
{
	if (index < 0 || index >= lattice_cell_count) throw std::out_of_range("cube lattice index is out of range");
	const int cell = (index * 317) % lattice_cell_count;
	return {static_cast<float>(cell % 32) - 15.5f, 0.3f,
		static_cast<float>(cell / 32) - 15.5f};
}

// Computes orientation directly from the simulation tick, without accumulated
// state, so repeated workers produce the same pose for a given tick.
inline glm::quat rotation_for_tick(std::uint64_t tick)
{
	const double angle = std::fmod(static_cast<double>(tick) * (0.5 / 60.0), 2.0 * std::numbers::pi);
	return glm::angleAxis(static_cast<float>(angle), glm::vec3(0.0f, 1.0f, 0.0f));
}

// Checks all corners of every cube in the maximum workload against Vulkan's
// left-handed, zero-to-one clip volume.
inline bool cube_grid_in_frustum(const glm::mat4& view_projection)
{
	constexpr float half_extent = 0.3f;
	for (int i = 0; i < maximum_cube_count; ++i)
	{
		const glm::vec3 center = cube_position(i);
		for (int x : {-1, 1})
			for (int y : {-1, 1})
				for (int z : {-1, 1})
				{
					const glm::vec3 corner = center + glm::vec3(x, y, z) * half_extent;
					const glm::vec4 clip = view_projection * glm::vec4(corner, 1.0f);
					if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z)
						|| !std::isfinite(clip.w) || clip.w <= 0.0f || clip.x < -clip.w || clip.x > clip.w
						|| clip.y < -clip.w || clip.y > clip.w || clip.z < 0.0f || clip.z > clip.w)
						return false;
				}
	}
	return true;
}
} // namespace BenchmarkScene
