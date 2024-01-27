#pragma once

#include "identifications.hpp"

#include <unordered_set>
#include <queue>
#include <utility>


class EntityDeletionQueue
{
public:
	using DeletionCounter = uint32_t;

	uint32_t get_counter() const { return deletion_counter; }

	bool push(ObjectID id)
	{
		if (entities_in_q.find(id) != entities_in_q.end())
		{
			return false;
		}

		entities_in_q.insert(id);
		entities_to_delete.push({id, deletion_counter++});

		return true;
	}

	void pop()
	{
		if (entities_to_delete.empty())
		{
			return;
		}

		entities_in_q.erase(entities_to_delete.front().first);
		entities_to_delete.pop();
	}

	std::pair<ObjectID, DeletionCounter> front() const
	{
		return entities_to_delete.front();
	}

	bool empty()
	{
		return entities_to_delete.empty();
	}

private:
	std::unordered_set<ObjectID> entities_in_q;
	std::queue<std::pair<ObjectID, DeletionCounter>> entities_to_delete;
	DeletionCounter deletion_counter = 0;
};