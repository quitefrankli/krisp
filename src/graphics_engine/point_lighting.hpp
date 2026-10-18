#pragma once

#include "render_frame.hpp"

#include <array>
#include <span>


inline constexpr float POINT_SHADOW_FAR_PLANE = 256.0f;
inline constexpr uint32_t POINT_SHADOW_RESOLUTION = 1024;

struct PointLightingData
{
	std::array<SDS::PointLightData, SDS::MAX_POINT_LIGHTS> lights{};
	uint32_t light_count = 0;
	uint32_t shadow_count = 0;
};

// Preserves snapshot order; enabled lights with nonzero radiance and shadows
// receive dense cube-array slots. Rejects invalid radiance, positions, or capacity.
PointLightingData prepare_point_lighting(std::span<const RenderLightState> lights);
