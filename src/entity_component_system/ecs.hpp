#pragma once

#include "skeletal.hpp"
#include "renderable_system.hpp"
#include "equipment.hpp"
#include "light_source.hpp"
#include "collider_ecs.hpp"
#include "clickable.hpp"
#include "hoverable.hpp"
#include "physics/physics.hpp"
#include "particle_system.hpp"
#include "tile_system.hpp"
#include "transformation_system.hpp"
#include "objects/object.hpp"
#include "material_system.hpp"
#include "mesh_system.hpp"

#include <unordered_map>
#include <memory>
#include <limits>


class ECS :
	public RenderableSystem,
	public SkeletalSystem,
	public SkeletalAnimationSystem,
	public EquipmentSystem,
	public LightSystem,
	public ColliderSystem,
	public ClickableSystem,
	public HoverableSystem,
	public PhysicsSystem,
	public ParticleSystem,
	public TileSystem,
	public TransformationSystem
{
public:
	ECS() = default;
	ECS(const ECS&) = delete;
	ECS& operator=(const ECS&) = delete;
	ECS(ECS&&) = default;
	ECS& operator=(ECS&&) = default;

	void process(const float delta_secs);

	virtual ECS& get_ecs() override { return *this; }
	virtual const ECS& get_ecs() const override { return *this; }
	MeshSystem& get_mesh_system() { return mesh_system; }
	const MeshSystem& get_mesh_system() const { return mesh_system; }
	MaterialSystem& get_material_system() { return material_system; }
	const MaterialSystem& get_material_system() const { return material_system; }

	// Used by GameEngine
	void add_object(Object& object);
	void remove_object(const ObjectID id);
	void reset_preserving_transient_transformations();

	// Used by ECSComponents
	Object& get_object(const ObjectID id);
	const Object& get_object(const ObjectID id) const;
	bool has_object(const ObjectID id) const { return objects.contains(id); }

	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

private:
	MeshSystem mesh_system;
	MaterialSystem material_system;
	std::unordered_map<ObjectID, Object*> objects;
};
