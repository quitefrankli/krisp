#pragma once

#include "pipeline_id.hpp"

template<typename T>
concept has_vertex_info = requires(T t)
{
	{ T::get_vertex_stride() } -> std::convertible_to<uint32_t>;
	{ T::get_vertex_pos_offset() } -> std::convertible_to<uint32_t>;
};

template<typename T>
concept Stencileable = has_vertex_info<T>;

template<typename T>
concept Wireframeable = has_vertex_info<T>;