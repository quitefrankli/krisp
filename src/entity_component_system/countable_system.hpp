#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>


template<typename IDType, typename ContentType>
class CountableSystem
{
private:
	class Handle
	{
	public:
		~Handle() noexcept
		{
			if (registered)
				get_global()._remove(id);
		}

	private:
		friend CountableSystem;
		Handle(IDType id, ContentType* content) noexcept : id(id), content(content) {}
		const IDType id;
		ContentType* const content;
		bool registered = false;
	};

public:
	using HandlePtr = std::shared_ptr<Handle>;

	static HandlePtr add(std::unique_ptr<ContentType>&& content)
	{
		auto& global = get_global();
		const IDType id = content->get_id();
		auto owner = std::shared_ptr<Handle>(new Handle(id, content.get()));
		global._add(std::move(content), owner);
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
		const std::lock_guard lock(global.registry_mutex);
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

	/**
	 * Reads an immutable resource directly through an owner.
	 *
	 * This does not access or lock the global registry. The caller's owner keeps
	 * the pointed-to resource alive for the complete read.
	 */
	static const ContentType& get(const HandlePtr& owner)
	{
		if (!owner || !owner->registered)
			throw std::runtime_error("CountableSystem::get: owner is empty");
		return *owner->content;
	}

	static ContentType& get(IDType id)
	{
		auto& global = get_global();
		const std::lock_guard lock(global.registry_mutex);
		return global._get(id);
	}

	static bool contains(IDType id)
	{
		auto& global = get_global();
		const std::lock_guard lock(global.registry_mutex);
		return global.contents.contains(id);
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
		const std::lock_guard lock(global.retired_mutex);
		std::vector<IDType> result;
		result.swap(global.retired);
		return result;
	}

private:
	void _add(std::unique_ptr<ContentType>&& content, const HandlePtr& owner)
	{
		const IDType id = content->get_id();
		const std::lock_guard lock(registry_mutex);
		assert(!contents.contains(id));
		bool content_inserted = false;
		bool owner_inserted = false;
		try
		{
			content_inserted = contents.emplace(id, std::move(content)).second;
			if (!content_inserted)
				throw std::runtime_error("CountableSystem::_add: duplicate id");
			owner_inserted = owners.emplace(id, owner).second;
			if (!owner_inserted)
				throw std::runtime_error("CountableSystem::_add: duplicate owner");
			owner->registered = true;
		}
		catch (...)
		{
			if (owner_inserted)
				owners.erase(id);
			if (content_inserted)
				contents.erase(id);
			throw;
		}
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
		{
			const std::lock_guard lock(registry_mutex);
			const auto found = owners.find(id);
			if (found != owners.end())
				owners.erase(found);
			contents.erase(id);
		}
		{
			const std::lock_guard lock(retired_mutex);
			retired.push_back(id);
		}
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
	// Registry access can race with final-owner release on another thread.
	std::mutex registry_mutex;
	// Graphics drains retirements independently of registry reads and writes.
	std::mutex retired_mutex;
};
