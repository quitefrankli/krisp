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
static_assert(offsetof(SDS::GlobalData, view_pos) == 512);
static_assert(offsetof(SDS::GlobalData, light_pos) == 528);
static_assert(offsetof(SDS::GlobalData, light_color) == 544);
static_assert(offsetof(SDS::GlobalData, light_intensity) == 556);
static_assert(offsetof(SDS::GlobalData, shadow_far_plane) == 560);
static_assert(sizeof(SDS::GlobalData) == 576);
