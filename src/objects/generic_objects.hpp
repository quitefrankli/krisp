#pragma once

#include "object.hpp"
#include "object_interfaces/clickable.hpp"


class GenericClickableObject : public Object, public IClickable
{
public:
	virtual ~GenericClickableObject() override {};

	GenericClickableObject(std::vector<ShapePtr>&& shapes) : IClickable(static_cast<Object&>(*this))
	{
		this->shapes = std::move(shapes);
	}

	GenericClickableObject(ShapePtr&& shape) : IClickable(static_cast<Object&>(*this))
	{
		this->shapes.push_back(std::move(shape));
	}
};