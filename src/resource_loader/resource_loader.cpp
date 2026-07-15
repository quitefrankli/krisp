#include "resource_loader.hpp"
#include "resource_loader_mesh.ipp"
#include "resource_loader_animation.ipp"
#include "resource_loader_material.ipp"
#include "entity_component_system/ecs.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/material_factory.hpp"
#include "utility.hpp"

#include <tiny_gltf.h>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <functional>
#include <unordered_map>

ResourceLoader ResourceLoader::global_resource_loader;

MaterialID ResourceLoader::fetch_texture(std::filesystem::path file_path)
{
	try
	{
		if (!std::filesystem::exists(file_path))
			file_path = Utility::get_texture(file_path.string());

		const auto file_str = file_path.lexically_normal().string();
		if (global_resource_loader.texture_name_to_mat_id.contains(file_str))
			return global_resource_loader.texture_name_to_mat_id.at(file_str);

		return global_resource_loader.load_texture(file_path);
	}
	catch (const ResourceLoadError&)
	{
		throw;
	}
	catch (const std::runtime_error& error)
	{
		throw ResourceLoadError(error.what());
	}
	catch (const std::out_of_range& error)
	{
		throw ResourceLoadError(error.what());
	}
}

namespace
{
Maths::Transform node_transform(const tinygltf::Node& node)
{
	Maths::Transform transform;
	if (!node.matrix.empty())
	{
		transform.set_mat4(glm::make_mat4(node.matrix.data()));
		return transform;
	}
	if (!node.translation.empty())
		transform.set_pos({ node.translation[0], node.translation[1], node.translation[2] });
	if (!node.rotation.empty())
		transform.set_orient({ node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2] });
	if (!node.scale.empty())
		transform.set_scale({ node.scale[0], node.scale[1], node.scale[2] });
	return transform;
}

struct NodeInstance
{
	int node_index;
	glm::mat4 world_transform;
};

std::vector<int> get_parent_nodes(const tinygltf::Model& model)
{
	std::vector<int> parents(model.nodes.size(), -1);
	for (int parent = 0; parent < static_cast<int>(model.nodes.size()); ++parent)
		for (const int child : model.nodes[parent].children)
			parents.at(child) = parent;
	return parents;
}

std::vector<NodeInstance> collect_mesh_nodes(const tinygltf::Model& model, const tinygltf::Scene& scene)
{
	std::vector<NodeInstance> nodes;
	std::function<void(int, const glm::mat4&)> visit = [&](const int index, const glm::mat4& parent)
	{
		const auto& node = model.nodes.at(index);
		const glm::mat4 world = parent * node_transform(node).get_mat4();
		if (node.mesh >= 0)
			nodes.push_back({ index, world });
		for (const int child : node.children)
			visit(child, world);
	};
	for (const int root : scene.nodes)
		visit(root, Maths::identity_mat);
	return nodes;
}

std::vector<Bone> load_bones(const tinygltf::Model& model, const int skin_index)
{
	const auto& skin = model.skins.at(skin_index);
	if (skin.joints.empty())
		throw std::runtime_error("ResourceLoader: skin has no joints");

	std::unordered_map<int, uint32_t> node_to_joint;
	for (uint32_t joint = 0; joint < skin.joints.size(); ++joint)
		node_to_joint.emplace(skin.joints[joint], joint);

	std::vector<Bone> bones(skin.joints.size());
	if (skin.inverseBindMatrices >= 0)
	{
		GltfImport::AccessorReader matrices(model, skin.inverseBindMatrices);
		if (matrices.accessor.type != TINYGLTF_TYPE_MAT4 ||
			matrices.accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
			matrices.accessor.count != skin.joints.size())
			throw std::runtime_error("ResourceLoader: inverse bind matrices must be float mat4 values matching joints");
		for (size_t joint = 0; joint < bones.size(); ++joint)
		{
			glm::mat4 matrix(1.0f);
			for (size_t component = 0; component < 16; ++component)
				reinterpret_cast<float*>(&matrix)[component] = matrices.number(joint, component);
			bones[joint].inverse_bind_pose.set_mat4(matrix);
		}
	}

	const auto parents = get_parent_nodes(model);
	for (uint32_t joint = 0; joint < skin.joints.size(); ++joint)
	{
		const int node_index = skin.joints[joint];
		auto& bone = bones[joint];
		bone.name = model.nodes.at(node_index).name;
		glm::mat4 local = node_transform(model.nodes.at(node_index)).get_mat4();
		int parent = parents.at(node_index);
		std::vector<int> intermediary_nodes;
		while (parent >= 0 && !node_to_joint.contains(parent))
		{
			intermediary_nodes.push_back(parent);
			parent = parents.at(parent);
		}
		// A scene node above a skeleton places the whole model; it is represented by
		// LoadedMesh::transform and must not become part of the root bone.  Helper
		// nodes between two joints, however, are part of the skeleton hierarchy.
		if (parent >= 0)
		{
			for (const int intermediary : intermediary_nodes)
				local = node_transform(model.nodes.at(intermediary)).get_mat4() * local;
		}
		bone.relative_transform.set_mat4(local);
		bone.original_transform = bone.relative_transform;
		bone.parent_node = parent >= 0 ? node_to_joint.at(parent) : Bone::NO_PARENT;
	}
	return bones;
}

void add_warning(ResourceLoader::LoadedModel& result, const ResourceLoader::LoadOptions& options, std::string message)
{
	if (options.strict)
		throw std::runtime_error(message);
	result.warnings.push_back({ std::move(message) });
}
}

ResourceLoader::LoadedModel ResourceLoader::load_model(
	std::filesystem::path file_path,
	const LoadOptions& options)
{
	try
	{
	if (!std::filesystem::exists(file_path))
		file_path = Utility::get_model(file_path.string());
	const bool is_glb = file_path.extension() == ".glb";
	if (!is_glb && file_path.extension() != ".gltf")
		throw std::runtime_error(fmt::format("ResourceLoader::load_model: unsupported file format: {}", file_path.string()));

	tinygltf::Model model;
	std::string err;
	std::string warn;
	tinygltf::TinyGLTF loader;
	const bool loaded = is_glb
		? loader.LoadBinaryFromFile(&model, &err, &warn, file_path.string())
		: loader.LoadASCIIFromFile(&model, &err, &warn, file_path.string());
	if (!loaded)
		throw std::runtime_error(fmt::format("ResourceLoader::load_model: failed to load '{}': {}", file_path.string(), err));
	if (model.scenes.empty())
		throw std::runtime_error("ResourceLoader::load_model: model contains no scenes");

	const int scene_index = options.scene_index.value_or(model.defaultScene >= 0 ? model.defaultScene : 0);
	if (scene_index < 0 || scene_index >= static_cast<int>(model.scenes.size()))
		throw std::runtime_error("ResourceLoader::load_model: requested scene is out of range");

	LoadedModel result;
	if (!warn.empty())
		result.warnings.push_back({ warn });
	global_resource_loader.gltf_material_to_mat_ids.clear();
	for (size_t material_index = 0; material_index < model.materials.size(); ++material_index)
	{
		const auto& normal_texture = model.materials[material_index].normalTexture;
		if (normal_texture.index >= 0 && std::abs(normal_texture.scale - 1.0) > 0.00001)
			add_warning(result, options, fmt::format(
				"ResourceLoader: material {} normalTexture.scale={} is not supported; using 1.0",
				material_index, normal_texture.scale));
	}

	std::unordered_map<int, std::vector<Bone>> skins;
	std::unordered_map<int, std::vector<AnimationID>> animations_by_skin;
	for (const NodeInstance& instance : collect_mesh_nodes(model, model.scenes[scene_index]))
	{
		const auto& node = model.nodes.at(instance.node_index);
		if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
			throw std::runtime_error("ResourceLoader::load_model: node references an invalid mesh");

		LoadedMesh loaded_mesh;
		loaded_mesh.name = node.name.empty() ? model.meshes[node.mesh].name : node.name;
		loaded_mesh.transform.set_mat4(instance.world_transform);
		loaded_mesh.source_node = instance.node_index;
		loaded_mesh.source_skin = node.skin;

		std::optional<SkeletonID> skeleton;
		if (node.skin >= 0)
		{
			if (node.skin >= static_cast<int>(model.skins.size()))
				throw std::runtime_error("ResourceLoader::load_model: node references an invalid skin");
			auto it = skins.find(node.skin);
			if (it == skins.end())
			{
				it = skins.emplace(node.skin, load_bones(model, node.skin)).first;
				if (!model.animations.empty())
					animations_by_skin[node.skin] = load_animations(model, it->second, node.skin);
				const auto& animation_ids = animations_by_skin[node.skin];
				result.animations.insert(result.animations.end(), animation_ids.begin(), animation_ids.end());
			}
			skeleton = ECS::get().add_skeleton(it->second);
		}

		for (const auto& primitive : model.meshes[node.mesh].primitives)
		{
			const auto position_it = primitive.attributes.find("POSITION");
			if (position_it == primitive.attributes.end())
				throw std::runtime_error("ResourceLoader: primitive is missing POSITION");
			const auto positions = GltfImport::read_vec3(model, position_it->second);
			auto indices = GltfImport::read_indices(model, primitive, positions.size());
			if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
				add_warning(result, options, "ResourceLoader: converted a non-triangle primitive to triangles");
			indices = GltfImport::triangles_from(primitive, std::move(indices), options.allow_non_triangle_primitives);

			std::vector<glm::vec3> normals;
			if (GltfImport::has_attribute(primitive, "NORMAL"))
				normals = GltfImport::read_vec3(model, primitive.attributes.at("NORMAL"));
			else if (options.generate_missing_normals)
			{
				add_warning(result, options, "ResourceLoader: generated missing normals");
				normals = GltfImport::generate_normals(positions, indices);
			}
			else
				throw std::runtime_error("ResourceLoader: primitive is missing NORMAL");
			if (positions.size() != normals.size())
				throw std::runtime_error("ResourceLoader: POSITION and NORMAL counts differ");

			const auto material_ids = global_resource_loader.load_material(primitive, model);
			const bool has_texcoords = GltfImport::has_attribute(primitive, "TEXCOORD_0");
			const bool textured = primitive.material >= 0 &&
				model.materials.at(primitive.material).pbrMetallicRoughness.baseColorTexture.index >= 0;
			const bool normal_mapped = primitive.material >= 0 &&
				model.materials.at(primitive.material).normalTexture.index >= 0;
			if (normal_mapped && !has_texcoords)
				throw std::runtime_error("ResourceLoader: normal-mapped primitive is missing TEXCOORD_0");

			auto texcoords = has_texcoords
				? GltfImport::read_vec2(model, primitive.attributes.at("TEXCOORD_0"))
				: std::vector<glm::vec2>(positions.size(), glm::vec2(0.0f));
			if (texcoords.size() != positions.size())
				throw std::runtime_error("ResourceLoader: POSITION and TEXCOORD_0 counts differ");

			std::vector<glm::vec4> tangents(positions.size(), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			std::optional<GltfImport::TangentRemap> tangent_remap;
			if (normal_mapped)
			{
				if (GltfImport::has_attribute(primitive, "TANGENT"))
				{
					tangents = GltfImport::read_vec4(model, primitive.attributes.at("TANGENT"));
					if (tangents.size() != positions.size())
						throw std::runtime_error("ResourceLoader: POSITION and TANGENT counts differ");
					for (auto& tangent : tangents)
					{
						const glm::vec3 tangent_xyz(tangent);
						if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z) ||
							!std::isfinite(tangent.w) || glm::length(tangent_xyz) < 0.00001f ||
							std::abs(std::abs(tangent.w) - 1.0f) > 0.00001f)
							throw std::runtime_error("ResourceLoader: primitive contains an invalid TANGENT");
						tangent = glm::vec4(glm::normalize(tangent_xyz), tangent.w < 0.0f ? -1.0f : 1.0f);
					}
				}
				else if (!options.generate_missing_tangents)
					throw std::runtime_error("ResourceLoader: normal-mapped primitive is missing TANGENT");
				else
				{
					add_warning(result, options, "ResourceLoader: generated missing tangents");
					tangent_remap = GltfImport::generate_tangents(positions, normals, texcoords, indices);
				}
			}

			Renderable renderable;
			MeshPtr mesh;
			if (skeleton.has_value())
			{
				if (!GltfImport::has_attribute(primitive, "JOINTS_0") || !GltfImport::has_attribute(primitive, "WEIGHTS_0"))
					throw std::runtime_error("ResourceLoader: skinned primitive is missing JOINTS_0 or WEIGHTS_0");
				auto joints = GltfImport::read_vec4(model, primitive.attributes.at("JOINTS_0"), false);
				auto weights = GltfImport::read_vec4(model, primitive.attributes.at("WEIGHTS_0"));
				if (texcoords.size() != positions.size() || joints.size() != positions.size() || weights.size() != positions.size())
					throw std::runtime_error("ResourceLoader: skinned vertex attribute counts differ");
				if (tangent_remap.has_value())
				{
					mesh = std::make_unique<SkinnedMesh>(
						load_skinned_vertices(positions, normals, texcoords, *tangent_remap, joints, weights),
						std::move(tangent_remap->indices));
				}
				else
					mesh = std::make_unique<SkinnedMesh>(
						load_skinned_vertices(positions, normals, texcoords, tangents, joints, weights),
						std::move(indices));
				renderable.skeleton_id = skeleton;
				renderable.pipeline_render_type = ERenderType::SKINNED;
			}
			else if (textured && has_texcoords)
			{
				if (tangent_remap.has_value())
					mesh = std::make_unique<TexMesh>(
						load_tex_vertices(positions, normals, texcoords, *tangent_remap),
						std::move(tangent_remap->indices));
				else
					mesh = std::make_unique<TexMesh>(
						load_tex_vertices(positions, normals, texcoords, tangents), std::move(indices));
				renderable.pipeline_render_type = ERenderType::STANDARD;
			}
			else
			{
				if (textured)
					add_warning(result, options, "ResourceLoader: textured primitive has no TEXCOORD_0; imported as a colour mesh");
				mesh = std::make_unique<ColorMesh>(load_color_vertices(positions, normals), std::move(indices));
				renderable.pipeline_render_type = ERenderType::COLOR;
			}
			renderable.mesh_id = MeshSystem::add(std::move(mesh));
			renderable.material_ids = material_ids;
			loaded_mesh.renderables.push_back(renderable);
		}
		result.meshes.push_back(std::move(loaded_mesh));
	}
	if (result.meshes.empty())
		add_warning(result, options, "ResourceLoader: selected scene contains no mesh nodes");
	return result;
	}
	catch (const ResourceLoadError&)
	{
		throw;
	}
	catch (const std::runtime_error& error)
	{
		throw ResourceLoadError(error.what());
	}
	catch (const std::out_of_range& error)
	{
		throw ResourceLoadError(error.what());
	}
}

ResourceLoader::LoadedModel ResourceLoader::load_model(std::filesystem::path file_path)
{
	LoadOptions options;
	options.generate_missing_tangents = true;
	return load_model(std::move(file_path), options);
}
