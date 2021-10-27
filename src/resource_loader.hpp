#pragma once

#include <string>


class Object;

class ResourceLoader
{
public:
	virtual void load_mesh(Object& object, const std::string& filename);
};