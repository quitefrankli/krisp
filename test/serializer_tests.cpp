#include <serialization/serializer.hpp>
#include <serialization/resource_provenance.hpp>
#include <identifications.hpp>
#include <objects/object.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <entity_component_system/material_system.hpp>
#include <renderable/material.hpp>

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

TEST(Serialization, ImportedRenderableUsesOnlyStableSourceSelectors)
{
	const MeshID mesh_id = MeshSystem::add(std::make_unique<ColorMesh>(
		ColorVertices{ SDS::ColorVertex{} }, VertexIndices{ 0 }));
	const MaterialID material_id = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const ImportedResourceProvenance mesh_origin{
		.source = "characters/robot.glb", .scene = 2, .node = 7, .primitive = 3, .skin = 1 };
	const ImportedResourceProvenance material_origin{
		.source = "characters/robot.glb", .scene = 2, .node = 7, .primitive = 3, .material = 4 };
	ResourceProvenance::register_mesh(mesh_id, mesh_origin);
	ResourceProvenance::register_material(material_id, material_origin);

	Object source(Renderable{ mesh_id, { material_id }, ERenderType::STANDARD });
	Serializer serializer;
	source.serialize(serializer);
	const std::string yaml = serializer.emit();
	const auto document = Deserializer::parse(yaml);
	const auto saved = document.child("renderables").elements().front();

	EXPECT_EQ(saved.keys(), (std::vector<std::string>{
		"mesh_source", "material_ids", "render_type", "alpha_mode", "alpha_cutoff", "opacity", "casts_shadow", "render_on_top" }));
	const auto mesh_source = saved.child("mesh_source");
	EXPECT_EQ(mesh_source.read<std::string>("path"), "characters/robot.glb");
	EXPECT_EQ(mesh_source.read<int>("scene"), 2);
	EXPECT_EQ(mesh_source.read<int>("node"), 7);
	EXPECT_EQ(mesh_source.read<int>("primitive"), 3);
	const auto material_source = saved.child("material_ids").elements().front();
	EXPECT_EQ(material_source.read<std::string>("path"), "characters/robot.glb");
	EXPECT_EQ(material_source.read<int>("primitive"), 3);
	EXPECT_EQ(yaml.find("mesh_id:"), std::string::npos);

	ResourceProvenance::clear();
	EXPECT_EQ(MeshSystem::unregister_owner(mesh_id), 0);
	EXPECT_EQ(MaterialSystem::unregister_owner(material_id), 0);
}

TEST(Serialization, ImportedRenderableDeserializerDefersResourceResolution)
{
	const MeshID placeholder{};
	ResourceProvenance::register_mesh(placeholder, { .source = "robot.glb", .scene = 0, .node = 1, .primitive = 2 });
	Object source(Renderable{ placeholder, {}, ERenderType::STANDARD });
	Serializer serializer;
	source.serialize(serializer);
	const auto document = Deserializer::parse(serializer.emit());

	Object restored;
	EXPECT_NO_THROW(restored.deserialize(document));
	ASSERT_EQ(restored.renderables.size(), 1);
	EXPECT_EQ(restored.renderables.front().mesh_id, MeshID{});
	EXPECT_TRUE(restored.renderables.front().material_ids.empty());
	ResourceProvenance::clear();
}
