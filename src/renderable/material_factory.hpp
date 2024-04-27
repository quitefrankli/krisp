#pragma once

#include "identifications.hpp"
#include "materials/materials.hpp"


class MaterialFactory
{
public:
	static MaterialID create_id(EMaterialType preset);
};