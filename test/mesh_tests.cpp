#include "test_helper.hpp"

#include <renderable/mesh_factory.hpp>
#include <entity_component_system/mesh_system.hpp>

#include <gtest/gtest.h>


TEST(MeshFactory, circle)
{
	auto circle = MeshFactory::circle(MeshFactory::EVertexType::COLOR, 8);
	ASSERT_EQ(circle->get_num_unique_vertices(), 8+1); // 1 is center
	ASSERT_EQ(circle->get_num_vertex_indices(), 24);
}

TEST(MeshFactory, cube)
{
	auto cube = MeshFactory::cube();
	ASSERT_EQ(cube->get_num_unique_vertices(), 28); // TODO: this should be 24 and even better 8
	ASSERT_EQ(cube->get_num_vertex_indices(), 42);
}

TEST(MeshFactory, check_no_duplicate_generation)
{
	const auto cube1_id = MeshFactory::cube_id();
	const auto cube2_id = MeshFactory::cube_id();

	ASSERT_EQ(cube1_id, cube2_id);

	const auto* cube1 = &MeshSystem::get(cube1_id);
	const auto* cube2 = &MeshSystem::get(cube2_id);

	ASSERT_EQ(cube1, cube2);
}

TEST(MeshFactory, check_different_id_when_different_params)
{
	const auto arrow1_id = MeshFactory::arrow_id(0.05, 8);
	const auto arrow2_id = MeshFactory::arrow_id(0.05, 16);
	const auto arrow3_id = MeshFactory::arrow_id(0.05, 16);

	ASSERT_NE(arrow1_id, arrow2_id);
	ASSERT_EQ(arrow2_id, arrow3_id);
}

TEST(MeshSystem, check_num_owners)
{
	const auto id1 = MeshSystem::add(MeshFactory::circle());
	ASSERT_EQ(MeshSystem::get_num_owners(id1), 0);
	MeshSystem::register_owner(id1);
	ASSERT_EQ(MeshSystem::get_num_owners(id1), 1);

	const auto id2 = MeshSystem::add(MeshFactory::circle());
	MeshSystem::register_owner(id2);
	ASSERT_EQ(MeshSystem::get_num_owners(id1), 1);
	ASSERT_EQ(MeshSystem::get_num_owners(id2), 1);

	MeshSystem::unregister_owner(id1);
	MeshSystem::register_owner(id2);
	ASSERT_EQ(MeshSystem::get_num_owners(id1), 0);
	ASSERT_EQ(MeshSystem::get_num_owners(id2), 2);
}

TEST(MeshSystem, permanently_owned)
{
	const auto id1 = MeshFactory::circle_id();
	ASSERT_EQ(MeshSystem::get_num_owners(id1), std::numeric_limits<uint32_t>::max());
}