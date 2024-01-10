#pragma once

#include "input.hpp"

#include <glm/glm.hpp>

#include <algorithm>


template<typename GameEngineT>
Mouse<GameEngineT>::Mouse(App::Window& ref_window) :
	window(ref_window)
{
}

template<typename GameEngineT>
glm::vec2 Mouse<GameEngineT>::update_pos()
{
	prev_pos = curr_pos;
	curr_pos = window.get_cursor_pos();

	return curr_pos;
}

template<typename GameEngineT>
glm::vec2 Mouse<GameEngineT>::get_orig_offset()
{
	return curr_pos - orig_pos;
}

template<typename GameEngineT>
glm::vec2 Mouse<GameEngineT>::get_curr_pos() const
{
	return window.get_cursor_pos();
}

template<typename GameEngineT>
glm::vec2 Mouse<GameEngineT>::get_prev_offset()
{
	return curr_pos - prev_pos;
}

template<typename GameEngineT>
bool Mouse<GameEngineT>::update_pos_on_significant_offset(const float min_offset)
{
	const auto temp_pos = window.get_cursor_pos();
	if (glm::distance(temp_pos, prev_pos) < min_offset)
	{
		return false;
	}

	prev_pos = curr_pos;
	curr_pos = temp_pos;

	return true;
}