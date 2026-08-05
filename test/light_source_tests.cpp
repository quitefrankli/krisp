#include <entity_component_system/light_source.hpp>
#include <serialization/serializer.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>


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

TEST(LightSystem, deserialization_rejects_invalid_radiance_without_mutating_state)
{
	LightSystem lights;
	lights.add_light_source(ObjectID(1), LightComponent{});
	const auto invalid = Deserializer::parse(
		"light_system:\n"
		"  - entity_id: 2\n"
		"    intensity: -1.0\n"
		"    color: { x: 1.0, y: 1.0, z: 1.0 }\n");

	EXPECT_THROW(lights.deserialize(invalid), SerializationError);
	EXPECT_NE(lights.get_light_component(ObjectID(1)), nullptr);
	EXPECT_EQ(lights.get_light_component(ObjectID(2)), nullptr);
}
