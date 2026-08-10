#include <entity_component_system/ecs.hpp>
#include <serialization/resource_provenance.hpp>
#include <serialization/serializer.hpp>
#include "serialization_test_helper.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>


TEST(RenderableSystem, composes_group_and_local_state_but_supports_standalone_renderables)
{
	ECS ecs;
	Object group;
	ecs.add_object(group);
	ecs.set_position(group.get_id(), { 2.0f, 0.0f, 0.0f });
	auto grouped = Renderable::make_default(ecs);
	grouped.local_transform.set_pos({ 0.0f, 3.0f, 0.0f });
	const auto grouped_id = ecs.add_renderable(std::move(grouped), group.get_id());
	auto standalone = Renderable::make_default(ecs);
	standalone.local_transform.set_pos({ 0.0f, 0.0f, 4.0f });
	const auto standalone_id = ecs.add_renderable(std::move(standalone));

	EXPECT_EQ(glm::vec3(ecs.get_renderable_transform(grouped_id)[3]), glm::vec3(2.0f, 3.0f, 0.0f));
	EXPECT_EQ(glm::vec3(ecs.get_renderable_transform(standalone_id)[3]), glm::vec3(0.0f, 0.0f, 4.0f));
	Maths::Transform changed_local_transform;
	changed_local_transform.set_pos({ 0.0f, 5.0f, 0.0f });
	ecs.set_renderable_local_transform(grouped_id, changed_local_transform);
	EXPECT_EQ(glm::vec3(ecs.get_renderable_transform(grouped_id)[3]), glm::vec3(2.0f, 5.0f, 0.0f));
	EXPECT_TRUE(ecs.get_renderable_visibility(grouped_id));
	group.set_visibility(false);
	EXPECT_FALSE(ecs.get_renderable_visibility(grouped_id));
	EXPECT_TRUE(ecs.get_renderable_visibility(standalone_id));
	ecs.set_renderable_visibility(standalone_id, false);
	EXPECT_FALSE(ecs.get_renderable_visibility(standalone_id));
}

TEST(RenderableSystem, rejects_resources_owned_by_another_ecs)
{
	ECS source;
	ECS target;
	auto renderable = Renderable::make_default(source);

	EXPECT_THROW(target.add_renderable(std::move(renderable)), std::invalid_argument);
}

TEST(RenderableSystem, rejects_invalid_per_instance_opacity)
{
	ECS ecs;
	for (const float opacity : {
		-0.01f, 1.01f, std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN() })
	{
		auto renderable = Renderable::make_default(ecs);
		renderable.opacity = opacity;
		EXPECT_THROW(ecs.add_renderable(std::move(renderable)), std::invalid_argument);
	}
}

TEST(RenderableSystem, enforces_per_renderable_skeleton_binding_and_shared_lifetime)
{
	ECS ecs;
	EXPECT_THROW(ecs.add_skeleton({}), std::invalid_argument);
	Bone bone;
	const auto skeleton = ecs.add_skeleton({ bone });
	ResourceProvenance::register_skeleton(skeleton, { .source = "shared.glb", .skin = 0 });
	auto missing_skeleton = Renderable::make_default(ecs);
	missing_skeleton.pipeline_render_type = ERenderType::SKINNED_COLOR;
	EXPECT_THROW(ecs.add_renderable(std::move(missing_skeleton)), std::invalid_argument);
	EXPECT_THROW(ecs.add_renderable(Renderable::make_default(ecs), {}, skeleton), std::invalid_argument);

	auto first = Renderable::make_default(ecs);
	first.pipeline_render_type = ERenderType::SKINNED_COLOR;
	auto second = Renderable::make_default(ecs);
	second.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto first_id = ecs.add_renderable(std::move(first), {}, skeleton);
	const auto second_id = ecs.add_renderable(std::move(second), {}, skeleton);
	EXPECT_THROW(ecs.remove_skeleton(skeleton), std::logic_error);
	EXPECT_TRUE(ecs.remove_renderable(first_id));
	EXPECT_TRUE(ecs.has_skeleton(skeleton));
	EXPECT_THROW(ecs.remove_skeleton(skeleton), std::logic_error);
	EXPECT_TRUE(ecs.remove_renderable(second_id));
	EXPECT_TRUE(ecs.remove_skeleton(skeleton));
	EXPECT_EQ(ResourceProvenance::skeleton(skeleton), nullptr);
}

TEST(RenderableSystem, replaces_structural_state_with_a_new_identity)
{
	static_assert(std::is_same_v<
		decltype(std::declval<ECS&>().get_renderable(RenderableID{})),
		const RenderableAttachment&>);
	ECS ecs;
	const auto id = ecs.add_renderable(Renderable::make_default(ecs));
	auto invalid = ecs.get_renderable(id).renderable;
	invalid.pipeline_render_type = ERenderType::SKINNED_COLOR;

	EXPECT_THROW(ecs.replace_renderable(id, std::move(invalid)), std::invalid_argument);
	EXPECT_EQ(
		ecs.get_renderable(id).renderable.pipeline_render_type,
		ERenderType::COLOR);

	auto replacement = ecs.get_renderable(id).renderable;
	replacement.casts_shadow = false;
	const RenderableID replacement_id =
		ecs.replace_renderable(id, std::move(replacement));
	EXPECT_NE(replacement_id, id);
	EXPECT_FALSE(ecs.has_renderable(id));
	EXPECT_FALSE(ecs.get_renderable(replacement_id).renderable.casts_shadow);
}

TEST(RenderableSystem, clones_renderable_attachment_into_an_object)
{
	ECS ecs;
	Object source_object;
	Object target_object;
	ecs.add_object(source_object);
	ecs.add_object(target_object);
	Bone bone;
	const auto skeleton = ecs.add_skeleton({ bone });
	auto source = Renderable::make_default(ecs);
	source.name = "Source mesh";
	source.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto source_id = ecs.add_renderable(
		std::move(source), source_object.get_id(), skeleton);
	ecs.set_renderable_visibility(source_id, false);

	const auto clone_id = ecs.clone_renderable(source_id, target_object.get_id());

	EXPECT_NE(clone_id, source_id);
	EXPECT_TRUE(ecs.has_renderable(source_id));
	const auto& clone = ecs.get_renderable(clone_id);
	EXPECT_EQ(clone.object_id, target_object.get_id());
	EXPECT_EQ(clone.skeleton_id, skeleton);
	EXPECT_FALSE(clone.visible);
	EXPECT_EQ(clone.renderable.name, "Source mesh");
	EXPECT_EQ(clone.renderable.mesh_owner, ecs.get_renderable(source_id).renderable.mesh_owner);
	EXPECT_EQ(clone.renderable.material_owners,
		ecs.get_renderable(source_id).renderable.material_owners);
}

TEST(RenderableSystem, replaces_with_a_clone_using_source_binding_and_target_visibility)
{
	ECS ecs;
	Object source_object;
	Object target_object;
	ecs.add_object(source_object);
	ecs.add_object(target_object);
	Bone bone;
	const auto skeleton = ecs.add_skeleton({ bone });
	auto source = Renderable::make_default(ecs);
	source.name = "Replacement mesh";
	source.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto source_id = ecs.add_renderable(
		std::move(source), source_object.get_id(), skeleton);
	const auto target_id = ecs.add_renderable(
		Renderable::make_default(ecs), target_object.get_id());
	ecs.set_renderable_visibility(target_id, false);

	const auto replacement_id = ecs.replace_renderable_with_clone(target_id, source_id);

	EXPECT_FALSE(ecs.has_renderable(target_id));
	EXPECT_TRUE(ecs.has_renderable(source_id));
	const auto& replacement = ecs.get_renderable(replacement_id);
	EXPECT_EQ(replacement.object_id, target_object.get_id());
	EXPECT_EQ(replacement.skeleton_id, skeleton);
	EXPECT_FALSE(replacement.visible);
	EXPECT_EQ(replacement.renderable.name, "Replacement mesh");
}

TEST(RenderableSystem, clone_failures_leave_renderables_unchanged)
{
	ECS ecs;
	Object target_object;
	ecs.add_object(target_object);
	const auto target_id = ecs.add_renderable(
		Renderable::make_default(ecs), target_object.get_id());
	const auto initial_ids = ecs.get_renderable_ids();

	EXPECT_THROW(
		ecs.clone_renderable(RenderableID(999999), target_object.get_id()),
		std::out_of_range);
	EXPECT_THROW(
		ecs.replace_renderable_with_clone(target_id, RenderableID(999999)),
		std::out_of_range);
	EXPECT_EQ(ecs.get_renderable_ids(), initial_ids);
}

TEST(RenderableSystem, clone_replacement_supports_the_same_source_and_target)
{
	ECS ecs;
	Object object;
	ecs.add_object(object);
	auto renderable = Renderable::make_default(ecs);
	renderable.name = "Self replacement";
	const auto id = ecs.add_renderable(std::move(renderable), object.get_id());

	const auto replacement_id = ecs.replace_renderable_with_clone(id, id);

	EXPECT_NE(replacement_id, id);
	EXPECT_FALSE(ecs.has_renderable(id));
	ASSERT_TRUE(ecs.has_renderable(replacement_id));
	EXPECT_EQ(ecs.get_renderable(replacement_id).object_id, object.get_id());
	EXPECT_EQ(ecs.get_renderable(replacement_id).renderable.name, "Self replacement");
}

TEST(RenderableSystem, clone_replacement_cleans_up_displaced_bone_attachments)
{
	ECS ecs;
	Object object;
	Object attached;
	ecs.add_object(object);
	ecs.add_object(attached);
	Bone bone;
	bone.name = "hand";
	const auto skeleton = ecs.add_skeleton({ bone });
	auto source = Renderable::make_default(ecs);
	source.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto source_id = ecs.add_renderable(std::move(source), {}, skeleton);
	auto target = Renderable::make_default(ecs);
	target.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto target_id = ecs.add_renderable(std::move(target), object.get_id(), skeleton);
	ASSERT_TRUE(ecs.attach_entity_to_bone(attached.get_id(), target_id, "hand"));

	ecs.replace_renderable_with_clone(target_id, source_id);

	EXPECT_FALSE(ecs.detach_entity_from_bone(attached.get_id()));
}

TEST(RenderableSystem, object_removal_cascades_only_its_renderables)
{
	ECS ecs;
	Object group;
	ecs.add_object(group);
	Bone bone;
	const auto skeleton = ecs.add_skeleton({ bone });
	auto grouped_payload = Renderable::make_default(ecs);
	grouped_payload.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const auto grouped = ecs.add_renderable(
		std::move(grouped_payload), group.get_id(), skeleton);
	const auto standalone = ecs.add_renderable(Renderable::make_default(ecs));

	ecs.remove_object(group.get_id());
	EXPECT_FALSE(ecs.has_renderable(grouped));
	EXPECT_TRUE(ecs.has_renderable(standalone));
	EXPECT_TRUE(ecs.has_skeleton(skeleton));
}

TEST(RenderableSystem, exact_source_renderable_controls_bone_attachment_transform)
{
	ECS ecs;
	Object character;
	Object item;
	ecs.add_object(character);
	ecs.add_object(item);
	ecs.set_position(character.get_id(), { 10.0f, 0.0f, 0.0f });
	Bone hand;
	hand.name = "hand";
	hand.relative_transform.set_pos({ 0.0f, 2.0f, 0.0f });
	const auto skeleton = ecs.add_skeleton({ hand });
	auto first = Renderable::make_default(ecs);
	first.pipeline_render_type = ERenderType::SKINNED_COLOR;
	first.local_transform.set_pos({ 1.0f, 0.0f, 0.0f });
	ecs.add_renderable(std::move(first), character.get_id(), skeleton);
	auto second = Renderable::make_default(ecs);
	second.pipeline_render_type = ERenderType::SKINNED_COLOR;
	second.local_transform.set_pos({ 4.0f, 0.0f, 0.0f });
	const auto source = ecs.add_renderable(std::move(second), character.get_id(), skeleton);
	Maths::Transform grip;
	grip.set_pos({ 0.0f, 0.0f, 3.0f });

	ASSERT_TRUE(ecs.attach_entity_to_bone(item.get_id(), source, "hand", grip));
	EXPECT_FALSE(ecs.attach_entity_to_bone(Entity(999999), source, "hand", grip));
	ecs.process(0.0f);
	EXPECT_EQ(ecs.get_position(item.get_id()), glm::vec3(14.0f, 2.0f, 3.0f));
}

TEST(RenderableSystem, reset_preserves_transient_grouped_attachments_and_ids)
{
	ECS ecs;
	Object transient;
	transient.set_transient(true);
	ecs.add_object(transient);
	const auto id = ecs.add_renderable(Renderable::make_default(ecs), transient.get_id());
	ecs.set_renderable_visibility(id, false);

	ecs.reset_preserving_transient_transformations();
	ASSERT_TRUE(ecs.has_object(transient.get_id()));
	ASSERT_TRUE(ecs.has_renderable(id));
	EXPECT_EQ(ecs.get_renderable(id).object_id, transient.get_id());
	EXPECT_FALSE(ecs.get_renderable(id).visible);
	EXPECT_FALSE(ecs.get_renderable_visibility(id));
}

TEST(RenderableSystem, deserialization_preserves_transient_grouped_attachments)
{
	ECS ecs;
	Object transient;
	transient.set_transient(true);
	ecs.add_object(transient);
	const auto id = ecs.add_renderable(Renderable::make_default(ecs), transient.get_id());
	Serializer empty;
	empty.sequence("renderable_system");

	deserialize_renderables(ecs, Deserializer::parse(empty.emit()));

	ASSERT_TRUE(ecs.has_renderable(id));
	EXPECT_EQ(ecs.get_renderable(id).object_id, transient.get_id());
}

TEST(RenderableSystem, serialization_restores_persistent_id_group_and_imported_skeleton)
{
	ECS ecs;
	Object group;
	ecs.add_object(group);
	Bone bone;
	const auto skeleton = ecs.add_skeleton({ bone });
	ResourceProvenance::register_skeleton(skeleton, {
		.source = "character.glb", .scene = 0, .node = 2, .skin = 1 });
	auto renderable = Renderable::make_default(ecs);
	renderable.pipeline_render_type = ERenderType::SKINNED_COLOR;
	renderable.shading_mode = EShadingMode::UNLIT;
	const auto id = ecs.add_renderable(std::move(renderable), group.get_id(), skeleton);
	const auto& payload = ecs.get_renderable(id).renderable;
	ResourceProvenance::register_mesh(payload.get_mesh_id(), {
		.source = "character.glb", .scene = 0, .node = 2, .primitive = 0 });
	for (const auto material : payload.get_material_ids())
		ResourceProvenance::register_material(material, {
			.source = "character.glb", .scene = 0, .node = 2, .primitive = 0 });
	Serializer serializer;
	serialize_renderables(ecs, serializer);
	ecs.set_renderable_visibility(id, false);

	deserialize_renderables(ecs, Deserializer::parse(serializer.emit()));
	const auto restored_ids = ecs.get_renderable_ids(group.get_id());
	ASSERT_EQ(restored_ids.size(), 1);
	EXPECT_NE(restored_ids.front(), id);
	EXPECT_EQ(ecs.get_renderable(restored_ids.front()).object_id, group.get_id());
	EXPECT_EQ(ecs.get_renderable(restored_ids.front()).skeleton_id, skeleton);
	EXPECT_EQ(ecs.get_renderable(restored_ids.front()).renderable.shading_mode,
		EShadingMode::UNLIT);
	EXPECT_TRUE(ecs.get_renderable(restored_ids.front()).visible);
}
