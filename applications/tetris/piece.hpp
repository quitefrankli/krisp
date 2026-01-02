#include <renderable/mesh_maths.hpp>
#include <renderable/mesh_factory.hpp>
#include <entity_component_system/mesh_system.hpp>
#include <entity_component_system/material_system.hpp>
#include <objects/object.hpp>
#include <game_engine.hpp>


enum class TetrisPieceType
{
	I,
	J,
	L,
	O,
	S,
	T,
	Z
};

class TetrisCell : public Object
{
public:
	TetrisCell(const glm::vec3& color)
	{
		Renderable& renderable = renderables.emplace_back();
		renderable.mesh_id = MeshFactory::cube_id();
		ColorMaterial material;
		material.data.diffuse = color;
		renderable.material_ids = { MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(material))) };
	}
};

class TetrisPiece : public Object
{
public:
	TetrisPiece(TetrisPieceType type, GameEngine& engine) :
		type(type)
	{
		static const std::vector<glm::vec3> standard_colors
		{
			glm::vec3(0.0f, 1.0f, 1.0f),
			glm::vec3(0.0f, 0.0f, 1.0f),
			glm::vec3(1.0f, 0.5f, 0.0f),
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3(1.0f, 0.0f, 1.0f),
			glm::vec3(1.0f, 0.0f, 0.0f)
		};

		switch (type)
		{
		case TetrisPieceType::I:
			spawn_cells(engine, { 12, 13, 14, 15 }, glm::vec3(-1.5f, 0.5f, 0.0f), standard_colors[0]);
			cell_locations = { { -1.5f, 0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f }, { 1.5f, 0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::J:
			spawn_cells(engine, { 8, 12, 13, 14 }, glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[1]);
			cell_locations = { { -1.0f, 1.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		case TetrisPieceType::L:
			spawn_cells(engine, { 10, 12, 13, 14 }, glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[2]);
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::O:
			spawn_cells(engine, { 8, 9, 12, 13 }, glm::vec3(-0.5f, -0.5f, 0.0f), standard_colors[3]);
			cell_locations = { { -0.5f, 0.5f }, { 0.5f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f } };
			type_specific_offset = glm::vec3(0.5f, 0.5f, 0.0f);
			break;
		case TetrisPieceType::S:
			spawn_cells(engine, { 9, 10, 12, 13 }, glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[4]);
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
			break;
		case TetrisPieceType::T:
			spawn_cells(engine, { 9, 12, 13, 14 }, glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[5]);
			cell_locations = { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } };
			break;
		case TetrisPieceType::Z:
			spawn_cells(engine, { 8, 9, 13, 14 }, glm::vec3(-1.0f, 0.0f, 0.0f), standard_colors[6]);
			cell_locations = { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
			break;
		default:
			throw std::runtime_error("Invalid TetrisPieceType");
		}	
	}

	glm::vec3 get_type_specific_offset() const { return type_specific_offset; }

	std::vector<glm::ivec2> get_cell_locations() const 
	{
		return get_cell_locations(get_transform());
	}

	std::vector<glm::ivec2> get_cell_locations(const glm::mat4& transform) const 
	{
		std::vector<glm::ivec2> result;
		for (auto& cell : cell_locations)
		{
			const auto transformed_cell = transform * glm::vec4(cell, 0.0f, 1.0f);
			result.emplace_back(std::round(transformed_cell.x), std::round(transformed_cell.y));
		}
		return result;
	}

	const std::vector<ObjectID>& get_cells() const { return cells; }

private:
	MeshID generate_mesh(const std::vector<int>& locations, 
					     // move so that origin matches the rotation point
					     const glm::vec3& translation)
	{
		// given a 4x4 grid, the locations are:
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15
		// for every location a cube will be inserted there
		// the origin of the grid is at the bottom left

		// translate_vertices(cube->get_vertices(), glm::vec3(-0.5f, -0.5f, 0.0f)); // center the cube at origin

		ColorVertices vertices;
		VertexIndices indices;
		for (auto location : locations)
		{
			const auto x = location % 4;
			const auto y = 3 - location / 4;

			const auto mesh = MeshFactory::cube();
			auto mesh_vertices = static_cast<const ColorMesh&>(*mesh).get_vertices();
			translate_vertices(mesh_vertices, glm::vec3(x, y, 0.0f) + translation);
			concatenate_vertices(vertices, indices, mesh_vertices, mesh->get_indices());
		}

		return MeshSystem::add(std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
	}

	MaterialID generate_material(const glm::vec3& color)
	{
		ColorMaterial material;
		material.data.diffuse = color;

		return MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(material)));
	}

	void spawn_cells(GameEngine& engine, 
					 const std::vector<int>& locations, 
					 // move so that origin matches the rotation point
					 const glm::vec3& translation,
					 const glm::vec3& color)
	{
		// given a 4x4 grid, the locations are:
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15
		// for every location a cube will be inserted there
		// the origin of the grid is at the bottom left

		for (auto location : locations)
		{
			const auto x = location % 4;
			const auto y = 3 - location / 4;

			auto& cell = engine.spawn_object<TetrisCell>(color);
			cell.set_position(glm::vec3(x, y, 0.0f) + translation);
			cell.attach_to(this);
			cells.push_back(cell.get_id());
		}
	}

private:
	std::vector<ObjectID> cells;
	// <x, y> location of the object == <0, 0>
	std::vector<glm::vec2> cell_locations;

	TetrisPieceType type;
	// some shapes namely I and O have a center of rotation that's not directly on a cell
	glm::vec3 type_specific_offset{};
};