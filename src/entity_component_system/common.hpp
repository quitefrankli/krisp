#pragma once

#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>
#include <stdexcept>
#include <memory>
#include <vector>


struct DetectedEntityCollision
{
	bool bCollided = false;
	EntityID id = EntityID(0);
	glm::vec3 intersection;
};

template<typename IDType, typename ContentType>
class CountableSystem
{
private:
	class Handle
	{
	public:
		~Handle() noexcept
		{
			get_global()._remove(id);
		}

	private:
		friend CountableSystem;
		explicit Handle(IDType id) noexcept : id(id) {}
		const IDType id;
	};

public:
	using HandlePtr = std::shared_ptr<Handle>;

	static HandlePtr add(std::unique_ptr<ContentType>&& content)
	{
		const auto id = get_global()._add(std::move(content));
		auto owner = std::shared_ptr<Handle>(new Handle(id));
		get_global().owners.emplace(id, owner);
		return owner;
	}

	/**
	 * Acquires shared ownership of an existing resource.
	 *
	 * IDs are non-owning handles. The returned owner keeps the resource alive
	 * until it and every other owner are released. This is intended for
	 * consumers, such as the ECS, that receive an ID and must retain its
	 * resource. Throws std::runtime_error if the ID is missing or no live owner
	 * can be acquired.
	 */
	static HandlePtr acquire(IDType id)
	{
		auto& global = get_global();
		if (!global.contents.contains(id))
			throw std::runtime_error("CountableSystem::acquire: id not found");
		const auto found = global.owners.find(id);
		if (found == global.owners.end())
			throw std::runtime_error("CountableSystem::acquire: owner not found");
		auto owner = found->second.lock();
		if (!owner)
			throw std::runtime_error("CountableSystem::acquire: owner expired");
		return owner;
	}

	static IDType get_id(const HandlePtr& owner)
	{
		if (!owner)
			throw std::runtime_error("CountableSystem::get_id: owner is empty");
		return owner->id;
	}

	static ContentType& get(IDType id)
	{
		return get_global()._get(id);
	}

	static bool contains(IDType id)
	{
		return get_global().contents.contains(id);
	}

	/**
	 * Drains and returns IDs whose final owner has been released.
	 *
	 * The corresponding CPU-side resources have already been removed. The
	 * returned IDs allow consumers such as the graphics backend to retire their
	 * associated resources. Each retirement is returned once; a subsequent call
	 * returns an empty vector until more owners are released.
	 */
	static std::vector<IDType> take_retired()
	{
		auto& global = get_global();
		std::vector<IDType> result;
		result.swap(global.retired);
		return result;
	}

private:
	IDType _add(std::unique_ptr<ContentType>&& content)
	{
		const IDType id = content->get_id();
		assert(!contents.contains(id));
		contents.emplace(id, std::move(content));

		return id;
	}

	ContentType& _get(IDType id)
	{
		if (!contents.contains(id))
		{
			throw std::runtime_error("CountableSystem::_get: id not found");
		}
		return *contents[id];
	}

	void _remove(IDType id) noexcept
	{
		const auto found = owners.find(id);
		if (found != owners.end())
			owners.erase(found);
		contents.erase(id);
		retired.push_back(id);
	}

	static CountableSystem& get_global()
	{
		static CountableSystem system;
		return system;
	}

private:
	std::unordered_map<IDType, std::unique_ptr<ContentType>> contents;
	std::unordered_map<IDType, std::weak_ptr<Handle>> owners;
	std::vector<IDType> retired;
};

class ECS;
class Serializer;
class Deserializer;
