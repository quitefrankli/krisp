#pragma once

#include "object.hpp"


// CubeMap is essentially a 6 sided texture useful for when you a 3D background
class CubeMap : public Object
{
public:
	CubeMap();

	virtual ERenderType get_render_type() const override { return ERenderType::CUBEMAP; }
};