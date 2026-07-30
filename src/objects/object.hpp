#pragma once

#include "renderable/render_types.hpp" // TODO: come up with a to decouiple this, having graphics engine code here is bad
#include "identifications.hpp"
#include "renderable/renderable.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include <vector>
#include <string>
#include <utility>


class ResourceLoader;
class Serializer;
class Deserializer;

class Object
{
public:
	static constexpr std::string_view serialization_type_name = "Object";
	Object() = default;
	Object(const Renderable& renderable) :
		renderables({ renderable })
	{
	}
	Object(Renderable&& renderable) :
		renderables({ std::move(renderable) })
	{
	}
	Object(const std::vector<Renderable>& renderables) :
		renderables(renderables)
	{
	}
	Object(std::vector<Renderable>&& renderables) :
		renderables(std::move(renderables))
	{
	}
	Object(const Object& object) = delete;
	Object(Object&& object) noexcept;
	virtual ~Object() = default;

	Object& operator=(const Object& object) = delete;

	ObjectID get_id() const { return id; }
	virtual std::string_view serialization_type() const { return serialization_type_name; }
	virtual void serialize(Serializer& out) const;
	virtual void deserialize(const Deserializer& in);

	virtual void toggle_visibility() { bVisible = !bVisible; }
	virtual void set_visibility(bool isVisible) { bVisible = isVisible; }
	bool get_visibility() const { return bVisible; }
	void set_transient(bool transient) { transient_object = transient; }
	bool is_transient() const { return transient_object; }

	const std::string& get_name() const { return name; }
	void set_name(const std::string_view name) { this->name = name; }

	std::vector<Renderable> renderables;

private:
	ObjectID id = ObjectID::generate_new_id();

	std::string name;

	bool bVisible = true;
	bool transient_object = false;
};
