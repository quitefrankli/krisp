#include <graphics_engine/resource_manager/graphics_buffer.hpp>

#include <gtest/gtest.h>


class GraphicsBufferFixture : public testing::Test
{
public:
    GraphicsBufferFixture() :
		buffer1(nullptr, nullptr, buffer_size_1),
		buffer2(nullptr, nullptr, buffer_size_2, alignment)
    {
    }

	const uint32_t buffer_size_1 = 100;
	const uint32_t buffer_size_2 = 100;
	const uint32_t alignment = 4;
	GraphicsBufferV2 buffer1;
	GraphicsBufferV2 buffer2;
};

TEST_F(GraphicsBufferFixture, partially_fill_buffer)
{
	uint32_t id = 0;
	ASSERT_EQ(buffer1.reserve_slot(id++, 10), 0);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 10);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 40);

	ASSERT_EQ(buffer1.get_offset(0), 0);
	ASSERT_EQ(buffer1.get_offset(1), 10);
	ASSERT_EQ(buffer1.get_offset(2), 40);

	buffer1.free_slot(0);
	buffer1.free_slot(1);
	buffer1.free_slot(2);

	EXPECT_THROW(buffer1.get_offset(0), std::runtime_error);
	EXPECT_THROW(buffer1.get_offset(1), std::runtime_error);
	EXPECT_THROW(buffer1.get_offset(2), std::runtime_error);
}

TEST_F(GraphicsBufferFixture, overfill_buffer)
{
	uint32_t id = 0;
	ASSERT_EQ(buffer1.reserve_slot(id++, 10), 0);
	ASSERT_EQ(buffer1.reserve_slot(id++, 90), 10);

	ASSERT_EQ(buffer1.get_offset(0), 0);
	ASSERT_EQ(buffer1.get_offset(1), 10);

	EXPECT_THROW(buffer1.reserve_slot(id++, 90), std::runtime_error);
}

TEST_F(GraphicsBufferFixture, buffer_simple_defragmentation)
{
	uint32_t id = 0;
	ASSERT_EQ(buffer1.reserve_slot(id++, 10), 0);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 10);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 40);

	// not enough room
	EXPECT_THROW(buffer1.reserve_slot(id++, 60), std::runtime_error);

	buffer1.free_slot(2);

	ASSERT_EQ(buffer1.reserve_slot(id++, 60), 40);
}

TEST_F(GraphicsBufferFixture, buffer_two_way_defragmentation)
{
	uint32_t id = 0;
	ASSERT_EQ(buffer1.reserve_slot(id++, 10), 0);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 10);
	ASSERT_EQ(buffer1.reserve_slot(id++, 30), 40);

	// not enough room
	EXPECT_THROW(buffer1.reserve_slot(id++, 100), std::runtime_error);
	buffer1.free_slot(2);
	EXPECT_THROW(buffer1.reserve_slot(id++, 100), std::runtime_error);
	buffer1.free_slot(0);
	EXPECT_THROW(buffer1.reserve_slot(id++, 100), std::runtime_error);
	buffer1.free_slot(1);

	ASSERT_EQ(buffer1.reserve_slot(id++, 100), 0);
}

TEST_F(GraphicsBufferFixture, alignment)
{
	uint32_t id = 0;
	ASSERT_EQ(buffer2.reserve_slot(id++, 10), 0);
	ASSERT_EQ(buffer2.reserve_slot(id++, 30), 12);
	ASSERT_EQ(buffer2.reserve_slot(id++, 30), 44);

	// without alignment this should not throw as we only occupy 70/100
	// however with alignment it occupies 76/100 hence a throw
	EXPECT_THROW(buffer2.reserve_slot(id++, 28), std::runtime_error);

	ASSERT_EQ(buffer2.reserve_slot(id++, 24), 76);
}