#include "test_helper.hpp"

#include <renderable/mesh_factory.hpp>
#include <renderable/mesh_maths.hpp>
#include <entity_component_system/mesh_system.hpp>

#include <gtest/gtest.h>

#include <array>
#include <barrier>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>


static_assert(std::is_same_v<
	decltype(std::declval<MeshSystem&>().get(std::declval<MeshID>())),
	const Mesh&>);

struct SharedMeshTestContent
{
	explicit SharedMeshTestContent(MeshID id) : id(id) {}
	MeshID get_id() const { return id; }
	MeshID id;
};


TEST(MeshFactory, circle)
{
	auto circle = MeshFactory::circle(MeshFactory::EVertexType::COLOR, 8);
	ASSERT_EQ(circle->get_num_unique_vertices(), 8+1); // 1 is center
	ASSERT_EQ(circle->get_num_vertex_indices(), 24);
}

TEST(MeshFactory, cube)
{
	auto cube = MeshFactory::cube();
	ASSERT_EQ(cube->get_num_unique_vertices(), 24);
	ASSERT_EQ(cube->get_num_vertex_indices(), 36);
}

TEST(MeshFactory, textured_cube_has_readable_face_uvs)
{
	const auto cube = MeshFactory::cube(MeshFactory::EVertexType::TEXTURE);
	const auto& textured = static_cast<const TexMesh&>(*cube);
	const std::array expected_faces{
		std::pair{ glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(-1.0f, 0.0f, 0.0f) },
		std::pair{ glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f) },
		std::pair{ glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f) },
		std::pair{ glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) },
		std::pair{ glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f) },
		std::pair{ glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f) },
	};

	ASSERT_EQ(textured.get_vertices().size(), expected_faces.size() * 4);
	for (size_t face = 0; face < expected_faces.size(); ++face)
	{
		const auto& [expected_normal, expected_u_direction] = expected_faces[face];
		const auto first = textured.get_vertices().begin() + static_cast<std::ptrdiff_t>(face * 4);
		for (auto vertex = first; vertex != first + 4; ++vertex)
		{
			EXPECT_TRUE(glm_equal(vertex->normal, expected_normal));
			EXPECT_TRUE(glm_equal(glm::vec3(vertex->tangent), expected_u_direction));
			EXPECT_FLOAT_EQ(vertex->tangent.w, 1.0f);
		}

		const auto u0 = std::ranges::find_if(first, first + 4, [](const SDS::TexVertex& vertex)
		{
			return vertex.texCoord == glm::vec2(0.0f, 0.0f);
		});
		const auto u1 = std::ranges::find_if(first, first + 4, [](const SDS::TexVertex& vertex)
		{
			return vertex.texCoord == glm::vec2(1.0f, 0.0f);
		});
		ASSERT_NE(u0, first + 4);
		ASSERT_NE(u1, first + 4);
		EXPECT_GT(glm::dot(u1->pos - u0->pos, expected_u_direction), 0.0f);
	}
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
	MeshSystem meshes;
	auto first = meshes.add(MeshFactory::cube());
	auto second = meshes.add(MeshFactory::cube());
	const MeshID first_id = first->get_id();
	const MeshID second_id = second->get_id();

	EXPECT_NE(first_id, second_id);
	EXPECT_NE(&meshes.get(first_id), &meshes.get(second_id));

	first.reset();
	EXPECT_FALSE(meshes.contains(first_id));
	EXPECT_TRUE(meshes.contains(second_id));
}

TEST(MeshSystem, stores_are_isolated)
{
	MeshSystem first_store;
	MeshSystem second_store;
	auto owner = first_store.add(MeshFactory::cube());
	const MeshID id = owner->get_id();

	EXPECT_TRUE(first_store.contains(id));
	EXPECT_TRUE(first_store.owns(owner));
	EXPECT_FALSE(second_store.contains(id));
	EXPECT_FALSE(second_store.owns(owner));
	EXPECT_THROW(second_store.acquire(id), std::runtime_error);
}

TEST(MeshSystem, concurrent_shared_add_reuses_the_same_owner)
{
	MeshSystem source;
	auto source_owner = source.add(MeshFactory::cube());
	auto content = source_owner->shared_content();
	MeshSystem destination;
	constexpr size_t thread_count = 8;
	std::barrier<> start(static_cast<std::ptrdiff_t>(thread_count));
	std::vector<MeshHandle> owners(thread_count);
	std::vector<std::thread> threads;
	threads.reserve(thread_count);
	for (size_t index = 0; index < thread_count; ++index)
	{
		threads.emplace_back([&, index]
		{
			start.arrive_and_wait();
			owners[index] = destination.add_shared(content);
		});
	}
	for (auto& thread : threads)
		thread.join();

	ASSERT_TRUE(owners.front());
	for (const auto& owner : owners)
		EXPECT_EQ(owner, owners.front());
	EXPECT_TRUE(destination.owns(owners.front()));
	EXPECT_EQ(&destination.get(content->get_id()), content.get());
}

TEST(MeshSystem, shared_add_rejects_different_content_with_an_existing_id)
{
	CountableSystem<MeshID, const SharedMeshTestContent> resources;
	const MeshID id = MeshID::generate_new_id();
	auto first_content = std::make_shared<const SharedMeshTestContent>(id);
	auto first_owner = resources.add_shared(first_content);
	auto different_content = std::make_shared<const SharedMeshTestContent>(id);

	EXPECT_THROW(resources.add_shared(different_content), std::runtime_error);
	EXPECT_EQ(resources.acquire(id), first_owner);
	EXPECT_EQ(&resources.get(id), first_content.get());
}

TEST(MeshFactory, check_different_id_when_different_params)
{
	MeshSystem meshes;
	const auto arrow1 = meshes.add(MeshFactory::arrow(0.05, 8));
	const auto arrow2 = meshes.add(MeshFactory::arrow(0.05, 16));
	const auto arrow3 = meshes.add(MeshFactory::arrow(0.05, 16));
	const auto arrow1_id = arrow1->get_id();
	const auto arrow2_id = arrow2->get_id();
	const auto arrow3_id = arrow3->get_id();

	ASSERT_NE(arrow1_id, arrow2_id);
	ASSERT_NE(arrow2_id, arrow3_id);
}

TEST(MeshSystem, check_num_owners)
{
	MeshSystem meshes;
	meshes.take_retired();
	auto first = meshes.add(MeshFactory::circle());
	const MeshID first_id = first->get_id();
	ASSERT_EQ(first.use_count(), 1);

	{
		auto second = meshes.acquire(first_id);
		ASSERT_EQ(second, first);
		ASSERT_EQ(first.use_count(), 2);

		auto moved = std::move(second);
		ASSERT_EQ(moved, first);
		ASSERT_EQ(first.use_count(), 2);
	}

	ASSERT_EQ(first.use_count(), 1);
	EXPECT_TRUE(meshes.take_retired().empty());

	first = {};
	EXPECT_FALSE(meshes.contains(first_id));
	EXPECT_EQ(meshes.take_retired(), (std::vector<MeshID>{ first_id }));
	EXPECT_TRUE(meshes.take_retired().empty());
}

TEST(MeshFactory, owner_controls_generated_mesh_lifetime)
{
	MeshSystem meshes;
	auto circle = meshes.add(MeshFactory::circle());
	const MeshID id = circle->get_id();
	EXPECT_TRUE(meshes.contains(id));
	circle.reset();
	EXPECT_FALSE(meshes.contains(id));
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
