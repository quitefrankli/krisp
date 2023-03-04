#pragma once

#include "maths.hpp"
#include "objects/object.hpp"


namespace OnClickDispatchers
{
	class IBaseDispatcher
	{
	public:
		virtual void dispatch_on_click(Object& object, const Maths::Ray& ray, const glm::vec3& intersection) = 0;
	};

	// For when in editor mode, we want to be able to click and drag objects around etc.
	class EditorModeDispatcher : public IBaseDispatcher
	{
	public:
		virtual void dispatch_on_click(Object& object, const Maths::Ray& ray, const glm::vec3& intersection) override;
	};
}

// For objects that can be "clicked" on by the user
// Classes that derive from this MUST also derive from "Object"
// This isn't the best way to do this, it adds too much extra boilerplate, we should
//	probably just have a "clickable" component that can be added to any object
class IClickable
{
public:
	IClickable(Object& object) : object(object) {}

	IClickable(const IClickable&) = delete;

	// "ray" as projected from the cursor
	virtual void on_click(OnClickDispatchers::IBaseDispatcher& dispatcher, const Maths::Ray& ray, const glm::vec3& intersection)
	{
		dispatcher.dispatch_on_click(object, ray, intersection);
	}

	bool check_click(const Maths::Ray& ray, glm::vec3& intersection) const
	{
		return object.check_collision(ray, intersection);
	}

private:
	Object& object;
};