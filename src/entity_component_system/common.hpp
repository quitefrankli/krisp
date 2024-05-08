#pragma once

#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>
#include <stdexcept>


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
	static IDType add(std::unique_ptr<ContentType>&& content, bool is_permanant = false)
	{
		return get_global()._add(std::move(content), is_permanant);
	}

	static ContentType& get(IDType id)
	{
		return get_global()._get(id);
	}

	static uint32_t get_num_owners(IDType id)
	{
		return get_global()._get_num_owners(id);
	}

	// returns the new owner count
	static uint32_t register_owner(IDType id)
	{
		return get_global()._increment_owners(id);
	}

	// returns the new owner count
	static uint32_t unregister_owner(IDType id)
	{
		return get_global()._decrement_owners(id);
	}

private:
	static constexpr uint32_t PERMANENTLY_OWNED = std::numeric_limits<uint32_t>::max();

	IDType _add(std::unique_ptr<ContentType>&& content, bool is_permanent)
	{
		const IDType id = content->get_id();
		assert(!contents.contains(id));
		contents.emplace(id, std::move(content));
		owners[id] = is_permanent ? PERMANENTLY_OWNED : 0;

		return id;
	}

	ContentType& _get(IDType id)
	{
		assert(contents.contains(id));
		return *contents[id];
	}

	uint32_t _get_num_owners(IDType id) const
	{
		return owners.contains(id) ? owners.at(id) : 0;
	}

	uint32_t _increment_owners(IDType id)
	{
		assert(owners.contains(id));
		if (owners[id] == PERMANENTLY_OWNED)
		{
			return PERMANENTLY_OWNED;
		}

		return ++owners[id];
	}

	uint32_t _decrement_owners(IDType id)
	{
		assert(owners.contains(id));
		const auto count = owners[id];
		if (count == 0)
		{
			throw std::runtime_error("CountableSystem::_decrement_owners: count < 0");
		} else if (count == PERMANENTLY_OWNED)
		{
			return PERMANENTLY_OWNED;
		} else if (count == 1)
		{
			contents.erase(id);
			owners.erase(id);
			return 0;
		} else
		{
			return --owners[id];
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
