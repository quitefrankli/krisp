#pragma once

#include "identifications.hpp"
#include "common.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>


struct LightComponent
{
	float intensity = 1.0f; // scene-linear point-light intensity
	glm::vec3 color = { 1.0f, 1.0f, 1.0f }; // linear RGB
};

// Throws std::invalid_argument unless intensity and linear RGB are finite and
// non-negative. Zero intensity and black lights are valid.
void validate_light_component(const LightComponent& light);

class LightSystem
{
public:
	void add_light_source(ObjectID id, const LightComponent& new_light);
	void remove_light_source(const ObjectID id) { lights.erase(id); }
	bool has_light_source() const { return !lights.empty(); }

	// TODO: this is a quick hack and relies on at least 1 light source in the container, delete this when we can support multiple light sources
	ObjectID get_global_light_source() const { return lights.begin()->first; }

	LightComponent* get_light_component(const ObjectID id)
	{ 
		auto comp = lights.find(id);
		return comp == lights.end() ? nullptr : &comp->second;
	}

	const LightComponent* get_light_component(const ObjectID id) const
	{ 
		auto comp = lights.find(id);
		return comp == lights.end() ? nullptr : &comp->second;
	}

	void serialize(Serializer& out) const;
	void deserialize(const Deserializer& in);

protected:
	void remove_entity(const ObjectID id) { lights.erase(id); }

private:
	std::unordered_map<ObjectID, LightComponent> lights;
};
