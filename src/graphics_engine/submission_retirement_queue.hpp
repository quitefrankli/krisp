#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <utility>
#include <vector>


// Monotonically increasing label assigned after a successful graphics queue
// submission. Serial 0 represents the state before any submission; completing a
// serial also completes every earlier serial because the queue executes in order.
using SubmissionSerial = std::uint64_t;

// Holds resources that may still be referenced by submitted GPU work. A batch
// must be tagged with the newest submission that could use it; release_completed
// transfers ownership back to the caller only after that submission has finished.
// This queue is expected to be owned and accessed by the graphics thread.
template<typename Batch> class SubmissionRetirementQueue
{
public:
	SubmissionRetirementQueue() = default;
	SubmissionRetirementQueue(const SubmissionRetirementQueue &) = delete;
	SubmissionRetirementQueue &operator=(const SubmissionRetirementQueue &) = delete;
	SubmissionRetirementQueue(SubmissionRetirementQueue &&) noexcept = default;
	SubmissionRetirementQueue &operator=(SubmissionRetirementQueue &&) noexcept = default;

	void enqueue(const SubmissionSerial serial, Batch batch)
	{
		entries.push_back(Entry{serial, std::move(batch)});
	}

	std::vector<Batch> release_completed(const SubmissionSerial completed_serial)
	{
		std::vector<Batch> released;
		for (auto entry = entries.begin(); entry != entries.end();)
		{
			if (entry->serial > completed_serial)
			{
				++entry;
				continue;
			}

			released.push_back(std::move(entry->batch));
			entry = entries.erase(entry);
		}
		return released;
	}

	// Shutdown escape hatch: the caller must first ensure that no submitted GPU
	// work can still reference any queued batch (normally by waiting for idle).
	std::vector<Batch> release_all()
	{
		std::vector<Batch> released;
		released.reserve(entries.size());
		for (auto &entry : entries)
			released.push_back(std::move(entry.batch));
		entries.clear();
		return released;
	}

	bool empty() const { return entries.empty(); }
	std::size_t size() const { return entries.size(); }

private:
	struct Entry
	{
		SubmissionSerial serial;
		Batch batch;
	};

	std::list<Entry> entries;
};
