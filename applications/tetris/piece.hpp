#include <renderable/mesh_factory.hpp>
#include <renderable/material.hpp>
#include <renderable/renderable.hpp>
#include <entity_component_system/ecs.hpp>
#include <objects/object.hpp>
#include <game_engine.hpp>

#include <stdexcept>


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
};

class TetrisPiece : public Object
{
public:
	TetrisPiece(TetrisPieceType type) : type(type) {}

	void initialize(GameEngine& engine)
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

		auto& ecs = engine.get_ecs();
		const auto mesh = ecs.get_mesh_system().add(MeshFactory::cube());
		const auto edge_mesh = ecs.get_mesh_system().add(MeshFactory::cube_edges());
		auto cube_material = std::make_unique<PbrMaterial>(glm::vec4(color, 1.0f), 0.0f, 0.7f);
		cube_material->data.emissive_factor = color;
		const auto material = ecs.get_material_system().add(std::move(cube_material));
		const auto edge_material = ecs.get_material_system().add(
			std::make_unique<PbrMaterial>(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.9f));

		for (auto location : locations)
		{
			const auto x = location % 4;
			const auto y = 3 - location / 4;

			auto& cell = engine.spawn_object<TetrisCell>();
			auto& transform = ecs.get_transformation(cell.get_id());
			transform.set_position(glm::vec3(x, y, 0.0f) + translation);
			transform.attach_to(get_id());
			engine.attach_renderable(cell.get_id(), Renderable{
				.mesh_owner = mesh, .material_owners = { material }});
			engine.attach_renderable(cell.get_id(), Renderable{
				.mesh_owner = edge_mesh, .material_owners = { edge_material }});
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
