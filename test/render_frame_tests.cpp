#include "render_frame.hpp"

#include "renderable/material.hpp"
#include "renderable/mesh_factory.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>


namespace
{
bool matrices_are_equal(const glm::mat4& lhs, const glm::mat4& rhs)
{
	constexpr float tolerance = 0.00001f;
	for (glm::length_t column = 0; column < 4; ++column)
		for (glm::length_t row = 0; row < 4; ++row)
			if (std::abs(lhs[column][row] - rhs[column][row]) > tolerance)
				return false;
	return true;
}

glm::mat4 translation(const glm::vec3& offset)
{
	return glm::translate(glm::mat4(1.0f), offset);
}
}


TEST(RenderFrame, composes_transform_hierarchies_independent_of_storage_order)
{
	const std::vector<glm::mat4> local_transforms{
		translation({ 3.0f, 0.0f, 0.0f }),
		translation({ 1.0f, 0.0f, 0.0f }),
		translation({ 2.0f, 0.0f, 0.0f }),
		translation({ 0.0f, 4.0f, 0.0f }),
	};
	const std::vector<uint32_t> parents{ 2, RENDER_FRAME_NO_PARENT, 1, RENDER_FRAME_NO_PARENT };

	const auto composed = compose_transform_hierarchy(local_transforms, parents);

	ASSERT_EQ(composed.size(), local_transforms.size());
	EXPECT_TRUE(matrices_are_equal(composed[0], translation({ 6.0f, 0.0f, 0.0f })));
	EXPECT_TRUE(matrices_are_equal(composed[1], translation({ 1.0f, 0.0f, 0.0f })));
	EXPECT_TRUE(matrices_are_equal(composed[2], translation({ 3.0f, 0.0f, 0.0f })));
	EXPECT_TRUE(matrices_are_equal(composed[3], translation({ 0.0f, 4.0f, 0.0f })));
}

TEST(RenderFrame, rejects_invalid_transform_hierarchies)
{
	const std::vector<glm::mat4> transforms(2, glm::mat4(1.0f));

	EXPECT_THROW(
		compose_transform_hierarchy(transforms, std::vector<uint32_t>{ RENDER_FRAME_NO_PARENT }),
		std::invalid_argument);
	EXPECT_THROW(
		compose_transform_hierarchy(transforms, std::vector<uint32_t>{ RENDER_FRAME_NO_PARENT, 2 }),
		std::invalid_argument);
	EXPECT_THROW(
		compose_transform_hierarchy(transforms, std::vector<uint32_t>{ 1, 0 }),
		std::invalid_argument);
}

TEST(RenderFrame, composes_bone_hierarchy_and_inverse_bind_poses)
{
	RenderSkeletonDefinition definition{
		.id = SkeletonID(8),
		.version = 3,
		.bones = {
			{ .parent_index = RENDER_FRAME_NO_PARENT, .inverse_bind_pose = translation({ -1.0f, 0.0f, 0.0f }) },
			{ .parent_index = 0, .inverse_bind_pose = translation({ 0.0f, 0.0f, 4.0f }) },
		},
	};
	const std::vector<glm::mat4> local_pose{
		translation({ 2.0f, 0.0f, 0.0f }),
		translation({ 0.0f, 3.0f, 0.0f }),
	};

	const auto bones = compose_bone_transforms(local_pose, definition);

	ASSERT_EQ(bones.size(), 2);
	EXPECT_TRUE(matrices_are_equal(bones[0].inverse_transform, definition.bones[0].inverse_bind_pose));
	EXPECT_TRUE(matrices_are_equal(bones[0].final_transform, translation({ 1.0f, 0.0f, 0.0f })));
	EXPECT_TRUE(matrices_are_equal(bones[1].inverse_transform, definition.bones[1].inverse_bind_pose));
	EXPECT_TRUE(matrices_are_equal(bones[1].final_transform, translation({ 2.0f, 3.0f, 4.0f })));
}

TEST(RenderFrame, immutable_definitions_retain_mesh_and_material_assets)
{
	auto mesh = MeshSystem::add(MeshFactory::cube());
	auto material = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh);
	const MaterialID material_id = MaterialSystem::get_id(material);

	auto definition = std::make_shared<const RenderObjectDefinition>(RenderObjectDefinition{
		.id = ObjectID(5),
		.version = 9,
		.renderables = {
			{
				.local_transform = translation({ 1.0f, 2.0f, 3.0f }),
				.mesh_owner = mesh,
				.material_owners = { material },
			},
		},
		.skeleton_id = SkeletonID(8),
	});
	mesh.reset();
	material.reset();

	static_assert(std::is_const_v<std::remove_reference_t<decltype(*definition)>>);
	static_assert(std::is_same_v<
		decltype(definition->renderables[0].get_mesh()), const Mesh&>);
	static_assert(std::is_same_v<
		decltype(definition->renderables[0].get_material(0)), const Material&>);
	EXPECT_EQ(definition->version, 9);
	EXPECT_EQ(definition->skeleton_id, SkeletonID(8));
	EXPECT_EQ(definition->renderables[0].get_mesh().get_id(), mesh_id);
	EXPECT_EQ(definition->renderables[0].get_material(0).get_id(), material_id);
	EXPECT_TRUE(MeshSystem::contains(mesh_id));
	EXPECT_TRUE(MaterialSystem::contains(material_id));

	definition.reset();
	EXPECT_FALSE(MeshSystem::contains(mesh_id));
	EXPECT_FALSE(MaterialSystem::contains(material_id));
}

TEST(RenderFrameMailbox, publishes_immutable_latest_completed_frame_pair)
{
	static_assert(std::is_same_v<RenderFramePtr::element_type, const RenderFrame>);
	static_assert(std::is_same_v<CompletedRenderFramesPtr::element_type, const CompletedRenderFrames>);

	RenderFrameMailbox mailbox;
	EXPECT_EQ(mailbox.load_latest(), nullptr);

	const auto first = std::make_shared<const RenderFrame>(RenderFrame{ .frame_number = 1 });
	mailbox.publish_completed(first);
	const auto first_publication = mailbox.load_latest();
	ASSERT_NE(first_publication, nullptr);
	EXPECT_EQ(first_publication->current, first);
	EXPECT_EQ(first_publication->previous, nullptr);

	const auto skipped = std::make_shared<const RenderFrame>(RenderFrame{ .frame_number = 2 });
	const auto latest = std::make_shared<const RenderFrame>(RenderFrame{ .frame_number = 3 });
	mailbox.publish_completed(skipped);
	mailbox.publish_completed(latest);

	const auto latest_publication = mailbox.load_latest();
	ASSERT_NE(latest_publication, nullptr);
	EXPECT_EQ(latest_publication->current, latest);
	EXPECT_EQ(latest_publication->previous, skipped);
	EXPECT_EQ(first_publication->current, first);
	EXPECT_EQ(first_publication->previous, nullptr);
}

TEST(RenderFrameMailbox, rejects_empty_completed_frames)
{
	RenderFrameMailbox mailbox;
	EXPECT_THROW(mailbox.publish_completed(nullptr), std::invalid_argument);
	EXPECT_EQ(mailbox.load_latest(), nullptr);
}
