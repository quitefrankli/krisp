#include "light_source.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

void validate_light_component(const LightComponent& light)
{
	if (!std::isfinite(light.intensity) || light.intensity < 0.0f)
		throw std::invalid_argument("Light intensity must be finite and non-negative");
	if (!std::isfinite(light.color.r) || light.color.r < 0.0f
		|| !std::isfinite(light.color.g) || light.color.g < 0.0f
		|| !std::isfinite(light.color.b) || light.color.b < 0.0f)
		throw std::invalid_argument(
			"Light linear RGB color must be finite and non-negative");
}

void LightSystem::add_light_source(
	const ObjectID id,
	const LightComponent& new_light)
{
	validate_light_component(new_light);
	lights.emplace(id, new_light);
}

void LightSystem::serialize(Serializer& out) const
{
	std::vector<ObjectID> ids;
	ids.reserve(lights.size());
	for (const auto& [id, _] : lights)
		ids.push_back(id);
	std::ranges::sort(ids);

	auto entries = out.sequence("light_system");
	for (const auto id : ids) {
		auto entry = entries.append_map();
		const auto& light = lights.at(id);
		entry.write("entity_id", id.get_underlying());
		entry.write("intensity", light.intensity);
		Serialization::write_vec3(entry, "color", light.color);
	}
}

void LightSystem::deserialize(const Deserializer& in)
{
	std::unordered_map<ObjectID, LightComponent> restored;
	const auto entries = in.child("light_system").elements();
	for (std::size_t index = 0; index < entries.size(); ++index) {
		const auto& entry = entries[index];
		const ObjectID id(entry.read<std::uint64_t>("entity_id"));
		LightComponent light;
		light.intensity = entry.read<float>("intensity");
		light.color = Serialization::read_vec3(entry, "color");
		try {
			validate_light_component(light);
		} catch (const std::invalid_argument& error) {
			throw SerializationError("Invalid light at $.light_system["
				+ std::to_string(index) + "]: " + error.what());
		}
		if (!restored.emplace(id, light).second) {
			throw SerializationError("Duplicate light entity at $.light_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	lights = std::move(restored);
}
