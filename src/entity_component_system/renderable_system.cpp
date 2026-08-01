#include "renderable_system.hpp"

#include "ecs.hpp"
#include "serialization/resource_provenance.hpp"
#include "serialization/serialization_helpers.hpp"
#include "serialization/serializer.hpp"

#include <algorithm>
#include <limits>
#include <ranges>
#include <stdexcept>


namespace
{
void write_source(Serializer out, const ImportedResourceProvenance& source)
{
	out.write("path", source.source);
	out.write("scene", source.scene);
	out.write("node", source.node);
	out.write("primitive", source.primitive);
	out.write("material", source.material);
	out.write("texture", source.texture);
	out.write("skin", source.skin);
	out.write("animation", source.animation);
}

ImportedResourceProvenance read_source(const Deserializer& in)
{
	return {
		.source = in.read<std::string>("path"),
		.scene = in.read<int>("scene"),
		.node = in.read<int>("node"),
		.primitive = in.read<int>("primitive"),
		.material = in.read<int>("material"),
		.texture = in.read<int>("texture"),
		.skin = in.read<int>("skin"),
		.animation = in.read<int>("animation"),
	};
}
}

void RenderableSystem::validate_attachment(
	const Renderable& renderable,
	const std::optional<ObjectID> object_id,
	const std::optional<SkeletonID> skeleton_id) const
{
	if (object_id && !get_ecs().has_object(*object_id))
		throw std::out_of_range("RenderableSystem: object group not found");
	if (skeleton_id && !get_ecs().has_skeleton(*skeleton_id))
		throw std::out_of_range("RenderableSystem: skeleton not found");
	if (is_skinned_render_type(renderable.pipeline_render_type) != skeleton_id.has_value())
		throw std::invalid_argument(
			"RenderableSystem: skinned renderables require exactly one skeleton binding");
	if (!get_ecs().get_mesh_system().owns(renderable.mesh_owner))
		throw std::invalid_argument("RenderableSystem: mesh belongs to another ECS");
	for (const auto& material : renderable.material_owners)
		if (!get_ecs().get_material_system().owns(material))
			throw std::invalid_argument("RenderableSystem: material belongs to another ECS");
}

RenderableID RenderableSystem::add_renderable(
	Renderable renderable,
	const std::optional<ObjectID> object_id,
	const std::optional<SkeletonID> skeleton_id)
{
	validate_attachment(renderable, object_id, skeleton_id);
	const auto id = RenderableID::generate_new_id();
	renderables.emplace(id, RenderableAttachment{
		.renderable = std::move(renderable),
		.object_id = object_id,
		.skeleton_id = skeleton_id,
	});
	return id;
}

std::vector<RenderableID> RenderableSystem::add_renderables(
	std::vector<Renderable> values,
	const std::optional<ObjectID> object_id,
	const std::optional<SkeletonID> skeleton_id)
{
	for (const auto& renderable : values)
		validate_attachment(renderable, object_id, skeleton_id);
	std::vector<RenderableID> ids;
	ids.reserve(values.size());
	for (auto& renderable : values)
		ids.push_back(add_renderable(std::move(renderable), object_id, skeleton_id));
	return ids;
}

std::vector<RenderableID> RenderableSystem::get_renderable_ids() const
{
	std::vector<RenderableID> ids;
	ids.reserve(renderables.size());
	for (const auto& [id, _] : renderables)
		ids.push_back(id);
	std::ranges::sort(ids);
	return ids;
}

std::vector<RenderableID> RenderableSystem::get_renderable_ids(const ObjectID object_id) const
{
	std::vector<RenderableID> ids;
	for (const auto& [id, attachment] : renderables)
		if (attachment.object_id == object_id)
			ids.push_back(id);
	std::ranges::sort(ids);
	return ids;
}

void RenderableSystem::notify_removing(const RenderableID id)
{
	get_ecs().SkeletalSystem::on_renderable_removed(id);
	get_ecs().EquipmentSystem::on_renderable_removed(id);
}

void RenderableSystem::notify_replacing(
	const RenderableID old_id, const RenderableID new_id)
{
	get_ecs().SkeletalSystem::on_renderable_replaced(old_id, new_id);
	get_ecs().EquipmentSystem::on_renderable_replaced(old_id, new_id);
}

bool RenderableSystem::remove_renderable(const RenderableID id)
{
	if (!renderables.contains(id))
		return false;
	notify_removing(id);
	renderables.erase(id);
	return true;
}

RenderableID RenderableSystem::replace_renderable(
	const RenderableID id, Renderable renderable)
{
	const auto& attachment = renderables.at(id);
	validate_attachment(renderable, attachment.object_id, attachment.skeleton_id);
	const auto object_id = attachment.object_id;
	const auto skeleton_id = attachment.skeleton_id;
	const bool visible = attachment.visible;
	const RenderableID replacement_id = RenderableID::generate_new_id();
	renderables.emplace(replacement_id, RenderableAttachment{
		.renderable = std::move(renderable),
		.object_id = object_id,
		.skeleton_id = skeleton_id,
		.visible = visible,
	});
	notify_replacing(id, replacement_id);
	renderables.erase(id);
	return replacement_id;
}

void RenderableSystem::set_renderable_local_transform(
	const RenderableID id, Maths::Transform transform)
{
	renderables.at(id).renderable.local_transform = std::move(transform);
}

void RenderableSystem::remove_object_renderables(const ObjectID id)
{
	for (const auto renderable_id : get_renderable_ids(id))
		remove_renderable(renderable_id);
}

RenderableSystem::AttachmentMap RenderableSystem::take_renderables_if(
	const std::function<bool(ObjectID)>& predicate)
{
	AttachmentMap taken;
	for (auto it = renderables.begin(); it != renderables.end();)
	{
		if (it->second.object_id && predicate(*it->second.object_id))
		{
			auto node = renderables.extract(it++);
			taken.insert(std::move(node));
		}
		else
			++it;
	}
	return taken;
}

void RenderableSystem::restore_renderables(AttachmentMap values)
{
	for (auto& node : values)
		renderables.insert(std::move(node));
}

void RenderableSystem::set_renderable_visibility(const RenderableID id, const bool visible)
{
	renderables.at(id).visible = visible;
}

glm::mat4 RenderableSystem::get_renderable_transform(const RenderableID id) const
{
	const auto& attachment = renderables.at(id);
	const auto local = attachment.renderable.local_transform.get_mat4();
	return attachment.object_id ? get_ecs().get_transform(*attachment.object_id) * local : local;
}

bool RenderableSystem::get_renderable_visibility(const RenderableID id) const
{
	const auto& attachment = renderables.at(id);
	return attachment.visible
		&& (!attachment.object_id || get_ecs().get_object(*attachment.object_id).get_visibility());
}

bool RenderableSystem::references_skeleton(const SkeletonID id) const
{
	return std::ranges::any_of(renderables, [id](const auto& entry) {
		return entry.second.skeleton_id == id;
	});
}

void RenderableSystem::serialize(Serializer& out) const
{
	auto entries = out.sequence("renderable_system");
	for (const auto id : get_renderable_ids())
	{
		const auto& attachment = renderables.at(id);
		if (attachment.object_id
			&& get_ecs().is_transient_transformation(*attachment.object_id))
			continue;
		auto entry = entries.append_map();
		entry.write("renderable_id", id.get_underlying());
		if (attachment.object_id)
			entry.write("object_id", attachment.object_id->get_underlying());
		else
			entry.write_null("object_id");
		if (attachment.skeleton_id)
		{
			if (const auto* source = ResourceProvenance::skeleton(*attachment.skeleton_id))
				write_source(entry.map("skeleton_source"), *source);
			else
				entry.write("skeleton_id", attachment.skeleton_id->get_underlying());
		}
		else
			entry.write_null("skeleton_id");
		entry.write("visible", attachment.visible);
		entry.write("render_type", static_cast<int>(attachment.renderable.pipeline_render_type));
		entry.write("alpha_mode", static_cast<int>(attachment.renderable.alpha_mode));
		entry.write("alpha_cutoff", attachment.renderable.alpha_cutoff);
		entry.write("opacity", attachment.renderable.opacity);
		entry.write("casts_shadow", attachment.renderable.casts_shadow);
		entry.write("render_on_top", attachment.renderable.render_on_top);
		Serialization::write_transform(entry, "local_transform", attachment.renderable.local_transform);

		const auto* mesh_source = ResourceProvenance::mesh(attachment.renderable.get_mesh_id());
		if (!mesh_source)
			throw SerializationError("Procedurally generated mesh at $.renderable_system");
		write_source(entry.map("mesh_source"), *mesh_source);
		auto materials = entry.sequence("material_sources");
		for (const auto material_id : attachment.renderable.get_material_ids())
		{
			const auto* material_source = ResourceProvenance::material(material_id);
			if (!material_source)
				throw SerializationError("Procedurally generated material at $.renderable_system");
			write_source(materials.append_map(), *material_source);
		}
	}
}

void RenderableSystem::deserialize(const Deserializer& in)
{
	std::unordered_map<RenderableID, RenderableAttachment> restored;
	const auto entries = in.child("renderable_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index)
	{
		const auto& entry = entries[index];
		const RenderableID id(entry.read<std::uint64_t>("renderable_id"));
		Renderable renderable;
		const auto mesh_id = ResourceProvenance::find_mesh(
			get_ecs().get_mesh_system(), read_source(entry.child("mesh_source")));
		if (!mesh_id)
			throw SerializationError("Missing imported mesh resource at " + entry.path());
		renderable.mesh_owner = get_ecs().get_mesh_system().acquire(*mesh_id);
		for (const auto& material : entry.child("material_sources").elements())
		{
			const auto material_id = ResourceProvenance::find_material(
				get_ecs().get_material_system(), read_source(material));
			if (!material_id)
				throw SerializationError("Missing imported material resource at " + material.path());
			renderable.material_owners.push_back(get_ecs().get_material_system().acquire(*material_id));
		}
		renderable.pipeline_render_type = static_cast<ERenderType>(entry.read<int>("render_type"));
		renderable.alpha_mode = static_cast<EAlphaMode>(entry.read<int>("alpha_mode"));
		renderable.alpha_cutoff = entry.read<float>("alpha_cutoff");
		renderable.opacity = entry.read<float>("opacity");
		renderable.casts_shadow = entry.read<bool>("casts_shadow");
		renderable.render_on_top = entry.read<bool>("render_on_top");
		renderable.local_transform = Serialization::read_transform(entry, "local_transform");
		const auto object = entry.child("object_id");
		const auto object_id = object.kind() == SerializationKind::Null
			? std::optional<ObjectID>{} : std::optional<ObjectID>{ ObjectID(object.as<std::uint64_t>()) };
		std::optional<SkeletonID> skeleton_id;
		const auto fields = entry.keys();
		if (std::ranges::find(fields, "skeleton_source") != fields.end())
		{
			skeleton_id = ResourceProvenance::find_skeleton(read_source(entry.child("skeleton_source")));
			if (!skeleton_id)
				throw SerializationError("Missing imported skeleton resource at " + entry.path());
		}
		else if (std::ranges::find(fields, "skeleton_id") != fields.end())
		{
			const auto skeleton = entry.child("skeleton_id");
			if (skeleton.kind() != SerializationKind::Null)
				skeleton_id = SkeletonID(skeleton.as<std::uint64_t>());
		}
		validate_attachment(renderable, object_id, skeleton_id);
		if (!restored.emplace(id, RenderableAttachment{
			.renderable = std::move(renderable),
			.object_id = object_id,
			.skeleton_id = skeleton_id,
			.visible = entry.read<bool>("visible"),
		}).second)
			throw SerializationError("Duplicate renderable ID at $.renderable_system["
				+ std::to_string(index) + "].renderable_id");
	}
	for (const auto& [id, attachment] : renderables)
		if (attachment.object_id
			&& get_ecs().is_transient_transformation(*attachment.object_id)
			&& restored.contains(id))
		{
			throw SerializationError(
				"Persistent renderable conflicts with transient renderable "
				+ std::to_string(id.get_underlying()));
		}
	if (!restored.empty())
	{
		const auto maximum = std::ranges::max_element(restored, {}, [](const auto& entry) {
			return entry.first.get_underlying();
		})->first.get_underlying();
		if (maximum == std::numeric_limits<std::uint64_t>::max())
			throw SerializationError("Cannot advance RenderableID counter beyond uint64 maximum");
		RenderableID::set_next_id(std::max(RenderableID::get_next_id(), maximum + 1));
	}
	for (auto& [id, attachment] : renderables)
	{
		if (attachment.object_id
			&& get_ecs().is_transient_transformation(*attachment.object_id))
		{
			restored.emplace(id, std::move(attachment));
		}
		else
			notify_removing(id);
	}
	renderables = std::move(restored);
}
