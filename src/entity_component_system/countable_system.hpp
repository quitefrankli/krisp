#pragma once

#include <cassert>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>


template<typename IDType, typename ContentType>
class CountableSystem
{
private:
	struct Registry;

public:
	class Handle
	{
	public:
		~Handle() noexcept
		{
			if (registered)
				registry->remove(id);
		}

		IDType get_id() const noexcept { return id; }
		const ContentType& get() const
		{
			if (!registered)
				throw std::runtime_error("CountableSystem::Handle::get: owner is not registered");
			return *content;
		}

	private:
		friend CountableSystem;
		Handle(std::shared_ptr<Registry> registry, IDType id, ContentType* content) noexcept :
			registry(std::move(registry)), id(id), content(content)
		{}

		std::shared_ptr<Registry> registry;
		const IDType id;
		ContentType* const content;
		bool registered = false;
	};

	using HandlePtr = std::shared_ptr<Handle>;

	CountableSystem() : registry(std::make_shared<Registry>()) {}
	CountableSystem(const CountableSystem&) = delete;
	CountableSystem& operator=(const CountableSystem&) = delete;
	CountableSystem(CountableSystem&&) noexcept = default;
	CountableSystem& operator=(CountableSystem&&) noexcept = default;

	HandlePtr add(std::unique_ptr<ContentType>&& content)
	{
		const IDType id = content->get_id();
		auto owner = std::shared_ptr<Handle>(new Handle(registry, id, content.get()));
		registry->add(std::move(content), owner);
		return owner;
	}

	/**
	 * Acquires shared ownership of an existing resource.
	 *
	 * IDs are non-owning handles. The returned owner keeps the resource alive
	 * until it and every other owner are released. Throws std::runtime_error if
	 * the ID is not registered in this store or has no live owner.
	 */
	HandlePtr acquire(IDType id) const
	{
		const std::lock_guard lock(registry->registry_mutex);
		if (!registry->contents.contains(id))
			throw std::runtime_error("CountableSystem::acquire: id not found");
		const auto found = registry->owners.find(id);
		if (found == registry->owners.end())
			throw std::runtime_error("CountableSystem::acquire: owner not found");
		auto owner = found->second.lock();
		if (!owner)
			throw std::runtime_error("CountableSystem::acquire: owner expired");
		return owner;
	}

	ContentType& get(IDType id)
	{
		const std::lock_guard lock(registry->registry_mutex);
		return registry->get(id);
	}

	const ContentType& get(IDType id) const
	{
		const std::lock_guard lock(registry->registry_mutex);
		return registry->get(id);
	}

	bool contains(IDType id) const
	{
		const std::lock_guard lock(registry->registry_mutex);
		return registry->contents.contains(id);
	}

	bool owns(const HandlePtr& owner) const noexcept
	{
		return owner && owner->registered && owner->registry == registry;
	}

	/** Drains IDs whose final owner has been released from this store. */
	std::vector<IDType> take_retired()
	{
		const std::lock_guard lock(registry->retired_mutex);
		std::vector<IDType> result;
		result.swap(registry->retired);
		return result;
	}

private:
	struct Registry
	{
		void add(std::unique_ptr<ContentType>&& content, const HandlePtr& owner)
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
					throw std::runtime_error("CountableSystem::add: duplicate id");
				owner_inserted = owners.emplace(id, owner).second;
				if (!owner_inserted)
					throw std::runtime_error("CountableSystem::add: duplicate owner");
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

		ContentType& get(IDType id)
		{
			const auto found = contents.find(id);
			if (found == contents.end())
				throw std::runtime_error("CountableSystem::get: id not found");
			return *found->second;
		}

		const ContentType& get(IDType id) const
		{
			const auto found = contents.find(id);
			if (found == contents.end())
				throw std::runtime_error("CountableSystem::get: id not found");
			return *found->second;
		}

		void remove(IDType id) noexcept
		{
			{
				const std::lock_guard lock(registry_mutex);
				owners.erase(id);
				contents.erase(id);
			}
			{
				const std::lock_guard lock(retired_mutex);
				retired.push_back(id);
			}
		}

		std::unordered_map<IDType, std::unique_ptr<ContentType>> contents;
		std::unordered_map<IDType, std::weak_ptr<Handle>> owners;
		std::vector<IDType> retired;
		mutable std::mutex registry_mutex;
		std::mutex retired_mutex;
	};

	std::shared_ptr<Registry> registry;
};
