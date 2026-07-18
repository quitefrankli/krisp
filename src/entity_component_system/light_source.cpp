#include "light_source.hpp"
#include "serialization/serialization_helpers.hpp"

#include <algorithm>
#include <vector>

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
		EcsSerialization::write_vec3(entry, "color", light.color);
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
		light.color = EcsSerialization::read_vec3(entry, "color");
		if (!restored.emplace(id, light).second) {
			throw SerializationError("Duplicate light entity at $.light_system["
				+ std::to_string(index) + "].entity_id");
		}
	}
	lights = std::move(restored);
}
