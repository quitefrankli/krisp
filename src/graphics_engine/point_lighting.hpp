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

// Preserves snapshot order and lighting; eligible lights receive dense
// cube-array slots when shadows are enabled. Rejects invalid light data/capacity.
PointLightingData prepare_point_lighting(
	std::span<const RenderLightState> lights, bool shadows_enabled = true);
