#pragma once

#include <objects/object.hpp>


class Satellite : public Object
{
public:
	Satellite()
	{

	}

private:
	float rotation_speed; // in degrees per second
};