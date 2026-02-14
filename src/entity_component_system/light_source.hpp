#pragma once

#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>


struct LightComponent
{
	float intensity = 1.0f;
	glm::vec3 color = { 1.0f, 0.9f, 0.2f };
};

class LightSystem
{
public:
	void add_light_source(const ObjectID id, const LightComponent& new_light) { lights.emplace(id, new_light); }
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

protected:
	void remove_entity(const ObjectID id) { lights.erase(id); }

private:
	std::unordered_map<ObjectID, LightComponent> lights;
};
