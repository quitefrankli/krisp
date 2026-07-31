#include "graphics_engine/submission_retirement_queue.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>


TEST(SubmissionRetirementQueue, retains_batches_until_their_submission_completes)
{
	SubmissionRetirementQueue<std::string> queue;
	queue.enqueue(SubmissionSerial{2}, "second");

	EXPECT_TRUE(queue.release_completed(SubmissionSerial{1}).empty());
	EXPECT_FALSE(queue.empty());
	EXPECT_EQ(queue.size(), 1);
}

TEST(SubmissionRetirementQueue, releases_completed_batches_in_enqueue_order)
{
	SubmissionRetirementQueue<std::string> queue;
	queue.enqueue(SubmissionSerial{1}, "first");
	queue.enqueue(SubmissionSerial{2}, "second");
	queue.enqueue(SubmissionSerial{3}, "third");

	EXPECT_EQ(queue.release_completed(SubmissionSerial{2}), (std::vector<std::string>{"first", "second"}));
	EXPECT_EQ(queue.size(), 1);
	EXPECT_EQ(queue.release_completed(SubmissionSerial{3}), (std::vector<std::string>{"third"}));
	EXPECT_TRUE(queue.empty());
}

TEST(SubmissionRetirementQueue, zero_and_equal_serials_are_completed_inclusively)
{
	SubmissionRetirementQueue<std::string> queue;
	queue.enqueue(SubmissionSerial{0}, "zero-a");
	queue.enqueue(SubmissionSerial{0}, "zero-b");
	queue.enqueue(SubmissionSerial{1}, "one");

	EXPECT_EQ(queue.release_completed(SubmissionSerial{0}), (std::vector<std::string>{"zero-a", "zero-b"}));
	EXPECT_EQ(queue.size(), 1);
}

TEST(SubmissionRetirementQueue, release_all_drains_every_batch)
{
	SubmissionRetirementQueue<std::string> queue;
	queue.enqueue(SubmissionSerial{4}, "first");
	queue.enqueue(SubmissionSerial{8}, "second");

	EXPECT_EQ(queue.release_all(), (std::vector<std::string>{"first", "second"}));
	EXPECT_TRUE(queue.empty());
	EXPECT_TRUE(queue.release_all().empty());
}

namespace
{
class MoveOnlyDestructionTracker
{
public:
	explicit MoveOnlyDestructionTracker(int &destruction_count) : destruction_count(&destruction_count) {}

	MoveOnlyDestructionTracker(const MoveOnlyDestructionTracker &) = delete;
	MoveOnlyDestructionTracker &operator=(const MoveOnlyDestructionTracker &) = delete;

	MoveOnlyDestructionTracker(MoveOnlyDestructionTracker &&other) noexcept :
		destruction_count(std::exchange(other.destruction_count, nullptr))
	{
	}

	MoveOnlyDestructionTracker &operator=(MoveOnlyDestructionTracker &&other) noexcept
	{
		if (this == &other)
			return *this;
		destroy();
		destruction_count = std::exchange(other.destruction_count, nullptr);
		return *this;
	}

	~MoveOnlyDestructionTracker() { destroy(); }

private:
	void destroy()
	{
		if (destruction_count != nullptr)
			++*destruction_count;
		destruction_count = nullptr;
	}

	int *destruction_count;
};
} // namespace

TEST(SubmissionRetirementQueue, move_only_batches_are_destroyed_exactly_once)
{
	int destruction_count = 0;
	SubmissionRetirementQueue<MoveOnlyDestructionTracker> queue;
	queue.enqueue(SubmissionSerial{1}, MoveOnlyDestructionTracker(destruction_count));

	{
		auto released = queue.release_completed(SubmissionSerial{1});
		EXPECT_EQ(released.size(), 1);
		EXPECT_EQ(destruction_count, 0);
	}

	EXPECT_EQ(destruction_count, 1);
	EXPECT_TRUE(queue.empty());
}
