#pragma once

#include "identifications.hpp"
#include "renderable/renderable.hpp"

#include <optional>
#include <functional>
#include <unordered_map>
#include <vector>


class ECS;
class Serializer;
class Deserializer;
class SceneResourceWriter;
class SceneResourceReader;

struct RenderableAttachment
{
	Renderable renderable;
	std::optional<ObjectID> object_id;
	std::optional<SkeletonID> skeleton_id;
	bool visible = true;
};

class RenderableSystem
{
public:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

	RenderableID add_renderable(
		Renderable renderable,
		std::optional<ObjectID> object_id = {},
		std::optional<SkeletonID> skeleton_id = {});
	std::vector<RenderableID> add_renderables(
		std::vector<Renderable> renderables,
		std::optional<ObjectID> object_id = {},
		std::optional<SkeletonID> skeleton_id = {});
	RenderableID clone_renderable(
		RenderableID source_id,
		std::optional<ObjectID> object_id = {});

	bool has_renderable(RenderableID id) const { return renderables.contains(id); }
	const RenderableAttachment& get_renderable(RenderableID id) const { return renderables.at(id); }
	std::vector<RenderableID> get_renderable_ids() const;
	std::vector<RenderableID> get_renderable_ids(ObjectID object_id) const;

	bool remove_renderable(RenderableID id);
	// Structural fields are immutable for an ID. Replacement preserves grouping,
	// skeleton binding, and visibility, but returns a fresh ID.
	RenderableID replace_renderable(RenderableID id, Renderable renderable);
	// Clones the source payload and skeleton binding into the target's object
	// group, preserves target visibility, and deletes the target.
	RenderableID replace_renderable_with_clone(
		RenderableID target_id, RenderableID source_id);
	void set_renderable_local_transform(RenderableID id, Maths::Transform transform);
	void set_renderable_visibility(RenderableID id, bool visible);
	glm::mat4 get_renderable_transform(RenderableID id) const;
	bool get_renderable_visibility(RenderableID id) const;

	bool references_skeleton(SkeletonID id) const;
	void serialize(Serializer& out, SceneResourceWriter& resources) const;
	void deserialize(const Deserializer& in, SceneResourceReader& resources);

protected:
	using AttachmentMap = std::unordered_map<RenderableID, RenderableAttachment>;
	AttachmentMap take_renderables_if(const std::function<bool(ObjectID)>& predicate);
	void restore_renderables(AttachmentMap values);
	void remove_object_renderables(ObjectID id);

private:
	void validate_attachment(const Renderable& renderable,
		std::optional<ObjectID> object_id, std::optional<SkeletonID> skeleton_id) const;
	void notify_removing(RenderableID id);
	void notify_replacing(RenderableID old_id, RenderableID new_id);

	AttachmentMap renderables;
};
