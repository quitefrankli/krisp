#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <utility>

enum class ResourceLoadStage
{
	Document,
	DependencyRead,
	ImageDecode,
	MeshConversion,
	MaterialConstruction,
	EcsAdoption,
};

struct ResourceLoadSample
{
	ResourceLoadStage stage;
	uint64_t begin_ns;
	uint64_t end_ns;
	uint64_t self_ns;
};

namespace ResourceLoadObservation
{
using Observer = std::function<void(const ResourceLoadSample &)>;

inline thread_local const Observer *active_observer = nullptr;

struct ActiveStage
{
	ActiveStage *parent = nullptr;
	uint64_t child_ns = 0;
};
inline thread_local ActiveStage *active_stage = nullptr;

inline uint64_t now_ns()
{
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

class Scope
{
public:
	explicit Scope(const Observer &observer) : previous(active_observer), previous_stage(active_stage)
	{
		active_observer = observer ? &observer : nullptr;
		active_stage = nullptr;
	}
	~Scope()
	{
		active_observer = previous;
		active_stage = previous_stage;
	}
	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;

private:
	const Observer *previous;
	ActiveStage *previous_stage;
};

class Stage
{
public:
	explicit Stage(ResourceLoadStage stage) : observer(active_observer), stage(stage)
	{
		if (observer)
		{
			begin = now_ns();
			parent = active_stage;
			active_stage = &timing;
		}
	}
	~Stage()
	{
		if (!observer)
			return;
		const auto end = now_ns();
		const auto elapsed = end - begin;
		active_stage = parent;
		if (parent)
			parent->child_ns += elapsed;
		try
		{
			const auto callback_begin = now_ns();
			static const Observer disabled_observer;
			{
				Scope disable_observation(disabled_observer);
				(*observer)({stage, begin, end, elapsed - timing.child_ns});
			}
			const auto callback_end = now_ns();
			if (parent)
				parent->child_ns += callback_end - callback_begin;
		} catch (...)
		{
			// Diagnostic callbacks must not change loading behavior.
			if (parent)
				parent->child_ns += now_ns() - end;
		}
	}
	Stage(const Stage &) = delete;
	Stage &operator=(const Stage &) = delete;

private:
	const Observer *observer;
	ResourceLoadStage stage;
	uint64_t begin = 0;
	ActiveStage timing;
	ActiveStage *parent = nullptr;
};
} // namespace ResourceLoadObservation
