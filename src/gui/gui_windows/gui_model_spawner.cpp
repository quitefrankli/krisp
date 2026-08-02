#include "gui_model_spawner.hpp"

#include "gui_window_helpers.hpp"

#include "entity_component_system/mesh_system.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "renderable/mesh_factory.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <algorithm>

namespace
{
template<typename MeshType>
MeshHandle bake_mesh_transform(
	MeshSystem& meshes, const MeshType& source, const glm::mat4& transform)
{
	auto vertices = source.get_vertices();
	const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(transform)));
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f));
		vertex.normal = glm::normalize(normal_transform * vertex.normal);
	}
	return meshes.add(std::make_unique<MeshType>(std::move(vertices), source.get_indices()));
}

MeshHandle bake_mesh_transform(
	MeshSystem& meshes, const MeshID mesh_id, const glm::mat4& transform)
{
	const Mesh& source = meshes.get(mesh_id);
	if (const auto* mesh = dynamic_cast<const ColorMesh*>(&source))
		return bake_mesh_transform(meshes, *mesh, transform);
	if (const auto* mesh = dynamic_cast<const TexMesh*>(&source))
		return bake_mesh_transform(meshes, *mesh, transform);
	if (const auto* mesh = dynamic_cast<const SkinnedMesh*>(&source))
		return bake_mesh_transform(meshes, *mesh, transform);
	throw std::runtime_error("GuiModelSpawner: unsupported mesh type");
}
}

using GuiWindowDetail::begin_italic_combo;
using GuiWindowDetail::draw_resource_load_error;
using GuiWindowDetail::report_resource_load_error;

GuiModelSpawner::GuiModelSpawner() :
	EngineUiWindow({ "model_spawner", "Model Spawner", GuiPanelDock::LEFT })
{
	refresh_models();
}

void GuiModelSpawner::refresh_models()
{
	std::optional<std::string> selected_path;
	if (selected_model.value >= 0 && selected_model.value < static_cast<int>(model_paths.size()))
		selected_path = model_paths[selected_model.value];

	model_paths = Utility::get_all_models();
	std::ranges::sort(model_paths);

	model_tree = GuiWindowDetail::build_resource_tree(model_paths);

	selected_model = 0;
	if (selected_path)
	{
		const auto selected = std::ranges::find(model_paths, *selected_path);
		if (selected != model_paths.end())
			selected_model = static_cast<int>(std::distance(model_paths.begin(), selected));
	}
	selected_model.changed = true;
	load_error.reset();
}

void GuiModelSpawner::process(GameEngine& engine)
{
	if (should_refresh_models)
	{
		should_refresh_models = false;
		refresh_models();
	}

	if (model_to_spawn)
	{
		const auto model_path = std::move(*model_to_spawn);
		model_to_spawn.reset();
		ResourceLoader::LoadedModel loaded_model;
		try
		{
			loaded_model = ResourceLoader::load_model(engine.get_ecs(), model_path);
			load_error.reset();
		}
		catch (const ResourceLoadError& error)
		{
			load_error = report_resource_load_error(
				fmt::format("Model Spawner failed to load '{}'", model_path), error);
			return;
		}

		const auto model_name = std::filesystem::path(model_path).stem().string();
		const bool contains_skinned_mesh = std::ranges::any_of(loaded_model.meshes, [](const auto& loaded_mesh)
		{
			return loaded_mesh.skeleton_id.has_value();
		});
		const bool merge_meshes = merge_imported_meshes.value && !contains_skinned_mesh;
		if (merge_imported_meshes.value && contains_skinned_mesh)
			LOG_WARNING(Utility::get_logger(), "GuiModelSpawner: cannot merge skinned model '{}' into one object", model_name);

		if (merge_meshes)
		{
			auto object = std::make_shared<Object>();
			object->set_name(model_name);
			std::vector<Renderable> renderables;
			for (auto& loaded_mesh : loaded_model.meshes)
			{
				for (size_t index = 0; index < loaded_mesh.renderables.size(); ++index)
				{
					auto renderable = loaded_mesh.renderables[index];
					auto baked_mesh = bake_mesh_transform(engine.get_ecs().get_mesh_system(),
						renderable.get_mesh_id(), renderable.local_transform.get_mat4());
					renderable.mesh_owner = std::move(baked_mesh);
					renderable.local_transform = {};
					renderables.push_back(std::move(renderable));
				}
			}
			Object& spawned_object = engine.spawn_object(std::move(object));
			engine.get_ecs().get_transformation(spawned_object.get_id())
				.set_transform(loaded_model.onload_transform.get_mat4());
			engine.attach_renderables(spawned_object.get_id(), std::move(renderables));
			engine.get_ecs().add_mesh_collider(spawned_object.get_id());
			engine.get_ecs().add_clickable_entity(spawned_object.get_id());
			return;
		}

		for (auto& loaded_mesh : loaded_model.meshes)
		{
			auto mesh = std::make_shared<Object>();
			mesh->set_name(loaded_mesh.name.empty() ? model_name : loaded_mesh.name);
			Object& object = engine.spawn_object(std::move(mesh));
			engine.get_ecs().get_transformation(object.get_id())
				.set_transform(loaded_model.onload_transform.get_mat4());
			engine.attach_renderables(
				object.get_id(), std::move(loaded_mesh.renderables), loaded_mesh.skeleton_id);
			engine.get_ecs().add_mesh_collider(object.get_id());
			engine.get_ecs().add_clickable_entity(object.get_id());
		}
	}
}

void GuiModelSpawner::draw()
{
	if (begin())
	{

	const bool dropdown_open = begin_italic_combo("##model", "(select model)");
	if (dropdown_open && !model_dropdown_open)
		should_refresh_models = true;
	model_dropdown_open = dropdown_open;

	if (dropdown_open)
	{
		const std::optional<size_t> current = selected_model.value >= 0
			&& selected_model.value < static_cast<int>(model_paths.size())
			? std::optional<size_t>(selected_model.value) : std::nullopt;
		if (const auto selected = GuiWindowDetail::draw_resource_tree(
			model_tree, model_paths, current))
		{
			selected_model = static_cast<int>(*selected);
			selected_model.changed = true;
			model_to_spawn = model_paths[*selected];
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Merge imported meshes into one object", &merge_imported_meshes.value);
	draw_resource_load_error(load_error);

	}
	end();
}
