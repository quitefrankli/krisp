#include <shared_data_structures.hpp>

#include <cstddef>
#include <type_traits>


static_assert(std::is_standard_layout_v<SDS::MaterialData>);
static_assert(alignof(SDS::MaterialData) == 16);
static_assert(sizeof(SDS::MaterialData) == 48);
static_assert(offsetof(SDS::MaterialData, base_color_factor) == 0);
static_assert(offsetof(SDS::MaterialData, emissive_factor) == 16);
static_assert(offsetof(SDS::MaterialData, metallic_factor) == 28);
static_assert(offsetof(SDS::MaterialData, roughness_factor) == 32);
static_assert(offsetof(SDS::MaterialData, texture_flags) == 40);

static_assert(std::is_standard_layout_v<SDS::AlphaMaterialData>);
static_assert(sizeof(SDS::AlphaMaterialData) == 20);

static_assert(std::is_standard_layout_v<SDS::GlobalData>);
static_assert(alignof(SDS::GlobalData) == 16);
static_assert(offsetof(SDS::GlobalData, view_pos) == 128);
static_assert(offsetof(SDS::GlobalData, light_count) == 140);
static_assert(offsetof(SDS::GlobalData, shadow_far_plane) == 144);
static_assert(offsetof(SDS::GlobalData, lights) == 160);
static_assert(sizeof(SDS::GlobalData) == 3488);

static_assert(std::is_standard_layout_v<SDS::PointLightData>);
static_assert(alignof(SDS::PointLightData) == 16);
static_assert(offsetof(SDS::PointLightData, shadow_view_proj_mats) == 0);
static_assert(offsetof(SDS::PointLightData, position) == 384);
static_assert(offsetof(SDS::PointLightData, intensity) == 396);
static_assert(offsetof(SDS::PointLightData, color) == 400);
static_assert(offsetof(SDS::PointLightData, shadow_slot) == 412);
static_assert(sizeof(SDS::PointLightData) == 416);

static_assert(std::is_standard_layout_v<SDS::RasterizationPushConstant>);
static_assert(offsetof(SDS::RasterizationPushConstant, light_index) == 20);
static_assert(sizeof(SDS::RasterizationPushConstant) == 24);
