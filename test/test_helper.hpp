#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fmt/core.h>


template<typename T>
inline void glm_print(const T& t)
{
	std::cout<<glm::to_string(t)<<'\n';
}

template<typename T>
inline constexpr bool glm_equal(const T& t1, const T& t2, const float epsilon = 0.001f)
{
	if (glm::all(glm::epsilonEqual(t1, t2, epsilon)))
	{
		return true;
	}

	// an orientation may have two different quarternions
	// the difference lies in direction of rotation to result in the oritentation
	if constexpr (std::is_same_v<T, glm::quat>)
	{
		if (glm::all(glm::epsilonEqual(t1, -t2, epsilon)))
		{
			return true;
		}
	}

	fmt::print("glm_equal error: {} != {}\n", glm::to_string(t1), glm::to_string(t2));
	
	return false;
}