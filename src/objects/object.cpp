#include "object.hpp"
#include "serialization/serializer.hpp"
#include "serialization/serialization_helpers.hpp"

#include "serialization/resource_provenance.hpp"

#include <algorithm>
#include <ranges>


Object::Object(Object&& other) noexcept :
	id(other.id),
	renderables(std::move(other.renderables)),
	name(std::move(other.name)),
	bVisible(other.bVisible),
	transient_object(other.transient_object)
{}

void Object::serialize(Serializer& out) const
{
	out.write("type", serialization_type());
	out.write("id", id.get_underlying());
	out.write("name", name);
	out.write("visible", bVisible);
	auto saved_renderables = out.sequence("renderables");
	for (const auto& renderable : renderables)
	{
		auto saved = saved_renderables.append_map();
		const MeshID mesh_id = renderable.get_mesh_id();
		if (const auto* origin = ResourceProvenance::mesh(mesh_id))
		{
			auto source = saved.map("mesh_source");
			source.write("path", origin->source);
			source.write("scene", origin->scene);
			source.write("node", origin->node);
			source.write("primitive", origin->primitive);
		}
		else
			throw SerializationError(
				"Procedurally generated meshes cannot be serialized at $.renderables");
		auto materials = saved.sequence("material_ids");
		for (const auto material : renderable.get_material_ids())
		{
			if (const auto* origin = ResourceProvenance::material(material))
			{
				auto source = materials.append_map();
				source.write("path", origin->source);
				source.write("scene", origin->scene);
				source.write("node", origin->node);
				source.write("primitive", origin->primitive);
			}
			else
				throw SerializationError(
					"Procedurally generated materials cannot be serialized at $.renderables");
		}
		saved.write("render_type", static_cast<int>(renderable.pipeline_render_type));
		saved.write("alpha_mode", static_cast<int>(renderable.alpha_mode));
		saved.write("alpha_cutoff", renderable.alpha_cutoff);
		saved.write("opacity", renderable.opacity);
		saved.write("casts_shadow", renderable.casts_shadow);
		saved.write("render_on_top", renderable.render_on_top);
		Serialization::write_transform(saved, "local_transform", renderable.local_transform);
	}
}

void Object::deserialize(const Deserializer& in)
{
	id = ObjectID(in.read<uint64_t>("id"));
	ObjectID::set_next_id(std::max(ObjectID::get_next_id(), id.get_underlying() + 1));
	name = in.read<std::string>("name");
	bVisible = in.read<bool>("visible");
	renderables.clear();
	for (const auto& saved : in.child("renderables").elements())
	{
		Renderable renderable;
		const auto keys = saved.keys();
		const bool imported_mesh = std::ranges::find(keys, "mesh_source") != keys.end();
		if (!imported_mesh)
			throw SerializationError(
				"Procedurally generated meshes cannot be deserialized at " + saved.path());
		for (const auto& material : saved.child("material_ids").elements())
			if (material.kind() == SerializationKind::Scalar)
				throw SerializationError(
					"Procedurally generated materials cannot be deserialized at " + material.path());
		renderable.pipeline_render_type = static_cast<ERenderType>(saved.read<int>("render_type"));
		renderable.alpha_mode = static_cast<EAlphaMode>(saved.read<int>("alpha_mode"));
		renderable.alpha_cutoff = saved.read<float>("alpha_cutoff");
		renderable.opacity = saved.read<float>("opacity");
		renderable.casts_shadow = saved.read<bool>("casts_shadow");
		renderable.render_on_top = saved.read<bool>("render_on_top");
		if (std::ranges::find(keys, "local_transform") != keys.end())
			renderable.local_transform = Serialization::read_transform(saved, "local_transform");
		renderables.push_back(std::move(renderable));
	}
}
