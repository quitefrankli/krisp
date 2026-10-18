#include <entity_component_system/light_source.hpp>
#include <serialization/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>


TEST(LightSystem, accepts_zero_and_rejects_non_finite_or_negative_radiance)
{
	LightSystem lights;
	EXPECT_NO_THROW(lights.add_light_source(
		ObjectID(1), LightComponent{ .intensity = 0.0f, .color = glm::vec3(0.0f) }));
	EXPECT_THROW(lights.add_light_source(
		ObjectID(2), LightComponent{ .intensity = -1.0f }), std::invalid_argument);
	EXPECT_THROW(lights.add_light_source(ObjectID(3), LightComponent{
		.intensity = 1.0f,
		.color = { std::numeric_limits<float>::infinity(), 1.0f, 1.0f },
	}), std::invalid_argument);
}

TEST(LightSystem, enforces_capacity_and_returns_ids_in_sorted_order)
{
	LightSystem lights;
	for (int id = SDS::MAX_POINT_LIGHTS; id > 0; --id)
		lights.add_light_source(ObjectID(static_cast<std::uint64_t>(id)), LightComponent{});

	const auto ids = lights.get_light_source_ids();
	ASSERT_EQ(ids.size(), SDS::MAX_POINT_LIGHTS);
	for (std::size_t index = 0; index < ids.size(); ++index)
		EXPECT_EQ(ids[index], ObjectID(index + 1));

	EXPECT_THROW(lights.add_light_source(
		ObjectID(SDS::MAX_POINT_LIGHTS + 1), LightComponent{}), std::invalid_argument);
	lights.remove_light_source(ObjectID(1));
	EXPECT_NO_THROW(lights.add_light_source(
		ObjectID(SDS::MAX_POINT_LIGHTS + 1), LightComponent{}));
}

TEST(LightSystem, deserialization_rejects_invalid_radiance_without_mutating_state)
{
	LightSystem lights;
	lights.add_light_source(ObjectID(1), LightComponent{});
	const auto invalid = Deserializer::parse(
		"light_system:\n"
		"  - entity_id: 2\n"
		"    intensity: -1.0\n"
		"    color: { x: 1.0, y: 1.0, z: 1.0 }\n"
		"    enabled: true\n"
		"    casts_shadow: true\n");

	EXPECT_THROW(lights.deserialize(invalid), SerializationError);
	EXPECT_NE(lights.get_light_component(ObjectID(1)), nullptr);
	EXPECT_EQ(lights.get_light_component(ObjectID(2)), nullptr);
}

TEST(LightSystem, serializes_required_light_flags)
{
	LightSystem lights;
	lights.add_light_source(ObjectID(7), LightComponent{
		.enabled = false,
		.casts_shadow = false,
	});
	Serializer serializer;
	lights.serialize(serializer);
	const auto saved = Deserializer::parse(serializer.emit());
	const auto entry = saved.child("light_system").elements().front();
	EXPECT_FALSE(entry.read<bool>("enabled"));
	EXPECT_FALSE(entry.read<bool>("casts_shadow"));

	LightSystem restored;
	restored.deserialize(saved);
	ASSERT_NE(restored.get_light_component(ObjectID(7)), nullptr);
	EXPECT_FALSE(restored.get_light_component(ObjectID(7))->enabled);
	EXPECT_FALSE(restored.get_light_component(ObjectID(7))->casts_shadow);
}

TEST(LightSystem, deserialization_rejects_excess_lights_without_mutating_state)
{
	LightSystem lights;
	lights.add_light_source(ObjectID(1), LightComponent{});
	std::string document = "light_system:\n";
	for (int index = 1; index <= SDS::MAX_POINT_LIGHTS + 1; ++index) {
		document += "  - entity_id: " + std::to_string(index + 1) + "\n"
			+ "    intensity: 1.0\n"
			+ "    color: { x: 1.0, y: 1.0, z: 1.0 }\n"
			+ "    enabled: true\n"
			+ "    casts_shadow: false\n";
	}

	EXPECT_THROW(lights.deserialize(Deserializer::parse(document)), SerializationError);
	ASSERT_EQ(lights.get_light_source_ids(), std::vector<ObjectID>{ ObjectID(1) });
}
