#pragma once

#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>


using EntityID = ObjectID;;

struct DetectedEntityCollision
{
	bool bCollided = false;
	EntityID id = EntityID(0);
	glm::vec3 intersection;
};

template<typename IDType, typename ContentType>
class CountableSystem
{
public:
	static IDType add(std::unique_ptr<ContentType>&& content)
	{
		return get_global()._add(std::move(content));
	}

	static ContentType& get(IDType id)
	{
		return get_global()._get(id);
	}

	static uint32_t get_num_owners(IDType id)
	{
		return get_global()._get_num_owners(id);
	}

	static void increment_owners(IDType id)
	{
		get_global()._increment_owners(id);
	}

	static void decrement_owners(IDType id)
	{
		get_global()._decrement_owners(id);
	}

private:
	IDType _add(std::unique_ptr<ContentType>&& content)
	{
		const IDType id = content->get_id();
		assert(contents.find(id) == contents.end());
		contents.emplace(id, std::move(content));
		owners[id] = 0;
		return id;
	}

	ContentType& _get(IDType id)
	{
		assert(contents.find(id) != contents.end());
		return *contents[id];
	}

	uint32_t _get_num_owners(IDType id) const
	{
		auto it = owners.find(id);
		return it != owners.end() ? it->second : 0;
	}

	void _increment_owners(IDType id)
	{
		assert(owners.find(id) != owners.end());
		owners[id]++;
	}

	void _decrement_owners(IDType id)
	{
		assert(owners.find(id) != owners.end());
		owners[id]--;
		if (owners[id] == 0)
		{
			contents.erase(id);
			owners.erase(id);
		}
	}

	static CountableSystem& get_global()
	{
		static CountableSystem system;
		return system;
	}

private:
	std::unordered_map<IDType, std::unique_ptr<ContentType>> contents;
	std::unordered_map<IDType, uint32_t> owners;
};

class ECS;
