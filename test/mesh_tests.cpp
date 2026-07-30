#include "test_helper.hpp"

#include <renderable/mesh_factory.hpp>
#include <renderable/mesh_maths.hpp>
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

TEST(MeshFactory, capsule_is_y_aligned_and_rests_at_zero)
{
	auto capsule = MeshFactory::capsule(0.5f, 2.0f, 16, 4);
	const AABB& bounds = capsule->get_pick_data().get_bounds();

	EXPECT_TRUE(glm_equal(bounds.min_bound, { -0.5f, 0.0f, -0.5f }));
	EXPECT_TRUE(glm_equal(bounds.max_bound, { 0.5f, 2.0f, 0.5f }));
}

TEST(MeshFactory, generated_meshes_have_independent_lifetimes)
{
	auto first = MeshSystem::add(MeshFactory::cube());
	auto second = MeshSystem::add(MeshFactory::cube());
	const MeshID first_id = MeshSystem::get_id(first);
	const MeshID second_id = MeshSystem::get_id(second);

	EXPECT_NE(first_id, second_id);
	EXPECT_NE(&MeshSystem::get(first_id), &MeshSystem::get(second_id));

	first.reset();
	EXPECT_FALSE(MeshSystem::contains(first_id));
	EXPECT_TRUE(MeshSystem::contains(second_id));
}

TEST(MeshFactory, check_different_id_when_different_params)
{
	const auto arrow1 = MeshSystem::add(MeshFactory::arrow(0.05, 8));
	const auto arrow2 = MeshSystem::add(MeshFactory::arrow(0.05, 16));
	const auto arrow3 = MeshSystem::add(MeshFactory::arrow(0.05, 16));
	const auto arrow1_id = MeshSystem::get_id(arrow1);
	const auto arrow2_id = MeshSystem::get_id(arrow2);
	const auto arrow3_id = MeshSystem::get_id(arrow3);

	ASSERT_NE(arrow1_id, arrow2_id);
	ASSERT_NE(arrow2_id, arrow3_id);
}

TEST(MeshSystem, check_num_owners)
{
	MeshSystem::take_retired();
	auto first = MeshSystem::add(MeshFactory::circle());
	const MeshID first_id = MeshSystem::get_id(first);
	ASSERT_EQ(first.use_count(), 1);

	{
		auto second = MeshSystem::acquire(first_id);
		ASSERT_EQ(second, first);
		ASSERT_EQ(first.use_count(), 2);

		auto moved = std::move(second);
		ASSERT_EQ(moved, first);
		ASSERT_EQ(first.use_count(), 2);
	}

	ASSERT_EQ(first.use_count(), 1);
	EXPECT_TRUE(MeshSystem::take_retired().empty());

	first = {};
	EXPECT_FALSE(MeshSystem::contains(first_id));
	EXPECT_EQ(MeshSystem::take_retired(), (std::vector<MeshID>{ first_id }));
	EXPECT_TRUE(MeshSystem::take_retired().empty());
}

TEST(MeshFactory, owner_controls_generated_mesh_lifetime)
{
	auto circle = MeshSystem::add(MeshFactory::circle());
	const MeshID id = MeshSystem::get_id(circle);
	EXPECT_TRUE(MeshSystem::contains(id));
	circle.reset();
	EXPECT_FALSE(MeshSystem::contains(id));
}

TEST(MeshMaths, normal_generation_rejects_incomplete_triangles)
{
	ColorVertices vertices(3);
	std::vector<uint32_t> indices{ 0, 1 };
	EXPECT_THROW(generate_normals(vertices, indices), std::invalid_argument);
}

TEST(MeshMaths, normal_generation_rejects_out_of_range_indices)
{
	ColorVertices vertices(3);
	std::vector<uint32_t> indices{ 0, 1, 3 };
	EXPECT_THROW(generate_normals(vertices, indices), std::out_of_range);
}

TEST(MeshMaths, normal_generation_handles_sparse_and_degenerate_vertices)
{
	ColorVertices vertices(4);
	vertices[0].pos = glm::vec3(0.0f);
	vertices[1].pos = glm::vec3(0.0f);
	vertices[2].pos = glm::vec3(0.0f);
	vertices[3].pos = glm::vec3(2.0f, 0.0f, 0.0f);
	std::vector<uint32_t> indices{ 0, 1, 2 };

	generate_normals(vertices, indices);

	for (const auto& vertex : vertices)
	{
		EXPECT_TRUE(std::isfinite(vertex.normal.x));
		EXPECT_TRUE(std::isfinite(vertex.normal.y));
		EXPECT_TRUE(std::isfinite(vertex.normal.z));
		EXPECT_TRUE(glm_equal(vertex.normal, Maths::up_vec));
	}
}
