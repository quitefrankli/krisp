#pragma once

#include <unordered_map>
#include <functional>


class IClickable;

// Abstraction of collections of object interfaces
class ObjectInterfacesMenagerie
{
public:
	// Make engine recognise an object is clickable
	void add_clickable(uint64_t id, IClickable* clickable) { clickables.emplace(id, clickable); }
	// Make engine forget an object is clickable
	void remove_clickable(uint64_t id) { clickables.erase(id); }

	// For early exit, pass a functor that can return false
	void execute_on_clickables(std::function<bool(IClickable*)> functor)
	{
		for (auto& [id, clickable] : clickables)
		{
			if (!functor(clickable))
			{
				return;
			}
		}
	}

protected:
	// removes all interfaces associated with id
	void erase_from_menagerie(uint64_t id)
	{
		remove_clickable(id);
	}

private:
	std::unordered_map<uint64_t, IClickable*> clickables;

};