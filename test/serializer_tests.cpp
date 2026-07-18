#include <serialization/serializer.hpp>
#include <identifications.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

TEST(Serialization, TraversesNestedMaterialDocument)
{
	Serializer serializer;
	auto materials = serializer.sequence("material_system");
	auto material = materials.append_map();
	material.write("material_id", std::uint64_t{ 42 });
	material.write("name", "brass");
	auto colour = material.sequence("colour");
	colour.append(0.8);
	colour.append(0.6);
	colour.append(0.2);

	const auto document = Deserializer::parse(serializer.emit());
	const auto material_elements = document.child("material_system").elements();
	ASSERT_EQ(material_elements.size(), 1);
	EXPECT_EQ(material_elements[0].read<std::uint64_t>("material_id"), 42);
	EXPECT_EQ(material_elements[0].read<std::string>("name"), "brass");
	EXPECT_EQ(material_elements[0].child("colour").elements()[1].as<double>(), 0.6);
	EXPECT_EQ(material_elements[0].kind(), SerializationKind::Mapping);
	EXPECT_EQ(material_elements[0].keys(), (std::vector<std::string>{ "material_id", "name", "colour" }));
}

TEST(Serialization, RoundTripsScalarBoundariesAndNull)
{
	Serializer serializer;
	serializer.write("signed_min", std::numeric_limits<std::int64_t>::min());
	serializer.write("unsigned_max", std::numeric_limits<std::uint64_t>::max());
	serializer.write("signed_byte", std::numeric_limits<std::int8_t>::min());
	serializer.write("unsigned_byte", std::numeric_limits<std::uint8_t>::max());
	serializer.write("enabled", true);
	serializer.write("ratio", -1.25);
	serializer.write("owned", std::string("text"));
	serializer.write_null("nothing");

	const auto document = Deserializer::parse(serializer.emit());
	EXPECT_EQ(document.read<std::int64_t>("signed_min"), std::numeric_limits<std::int64_t>::min());
	EXPECT_EQ(document.read<std::uint64_t>("unsigned_max"), std::numeric_limits<std::uint64_t>::max());
	EXPECT_EQ(document.read<std::int8_t>("signed_byte"), std::numeric_limits<std::int8_t>::min());
	EXPECT_EQ(document.read<std::uint8_t>("unsigned_byte"), std::numeric_limits<std::uint8_t>::max());
	EXPECT_TRUE(document.read<bool>("enabled"));
	EXPECT_EQ(document.read<double>("ratio"), -1.25);
	EXPECT_EQ(document.read<std::string>("owned"), "text");
	EXPECT_EQ(document.child("nothing").kind(), SerializationKind::Null);
}

TEST(Serialization, RoundTripsNestedSequences)
{
	Serializer serializer;
	auto outer = serializer.map("outer");
	auto values = outer.sequence("values");
	values.append(1);
	values.append_null();
	auto nested = values.append_sequence();
	nested.append("last");

	const auto document = Deserializer::parse(serializer.emit());
	const auto elements = document.child("outer").child("values").elements();
	ASSERT_EQ(elements.size(), 3);
	EXPECT_EQ(elements[0].as<int>(), 1);
	EXPECT_EQ(elements[1].kind(), SerializationKind::Null);
	EXPECT_EQ(elements[2].elements()[0].as<std::string>(), "last");
}

TEST(Serialization, ReportsPathsForInvalidOperations)
{
	Serializer serializer;
	auto materials = serializer.sequence("material_system");
	auto material = materials.append_map();
	material.write("material_id", 7);

	try {
		material.write("material_id", 8);
		FAIL() << "Expected duplicate write to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.material_system[0].material_id"), std::string::npos);
	}

	const auto document = Deserializer::parse(serializer.emit());
	EXPECT_THROW(document.child("missing"), SerializationError);
	try {
		document.child("material_system").elements()[0].read<bool>("material_id");
		FAIL() << "Expected type mismatch to fail";
	} catch (const SerializationError& error) {
		EXPECT_NE(std::string(error.what()).find("$.material_system[0].material_id"), std::string::npos);
	}
	EXPECT_THROW(Deserializer::parse("mapping: [unterminated"), SerializationError);
}

TEST(Serialization, GenericIdCounterCanBeSetExplicitly)
{
	using TestID = GenericID<class SerializationCounterTestTag>;
	TestID::set_next_id(42);
	EXPECT_EQ(TestID::get_next_id(), 42);
	EXPECT_EQ(TestID::generate_new_id().get_underlying(), 42);
	EXPECT_EQ(TestID::generate_new_id().get_underlying(), 43);
}
