#include "resource_loader.hpp"
#include "resource_loader_mesh.ipp"
#include "resource_loader_animation.ipp"
#include "resource_loader_material.ipp"
#include "entity_component_system/ecs.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/material_factory.hpp"
#include "serialization/resource_provenance.hpp"
#include "utility.hpp"

#include <tiny_gltf.h>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
template<typename Resolver>
std::filesystem::path resolve_resource_filename(std::string_view filename, Resolver resolver)
{
	try
	{
		return resolver(filename);
	}
	catch (const std::runtime_error& error)
	{
		throw ResourceLoadError(error.what());
	}
}
}

ResourceLoader ResourceLoader::global_resource_loader;

MaterialID ResourceLoader::fetch_texture(
	const std::string_view filename,
	const ETextureSemantic semantic)
{
	if (semantic == ETextureSemantic::COUNT)
		throw ResourceLoadError("ResourceLoader::fetch_texture: invalid texture semantic");
	const auto file_path = resolve_resource_filename(filename, Utility::get_texture);

	const auto file_str = file_path.lexically_normal().string();
	const size_t semantic_index = static_cast<size_t>(semantic);
	auto& cached = global_resource_loader.texture_name_to_mat_id[file_str][semantic_index];
	if (cached && MaterialSystem::contains(*cached))
	{
		MaterialSystem::register_owner(*cached);
		return *cached;
	}
	cached.reset();

	const auto material_id = global_resource_loader.load_texture(file_path, semantic);
	if (auto* texture = dynamic_cast<TextureMaterial*>(&MaterialSystem::get(material_id)))
		texture->source = filename;
	MaterialSystem::register_owner(material_id);
	return material_id;
}

namespace
{
struct GltfDocument
{
	tinygltf::Model model;
	std::string warning;
};

GltfDocument load_gltf_document(const std::filesystem::path& file_path)
{
	const bool is_glb = file_path.extension() == ".glb";
	if (!is_glb && file_path.extension() != ".gltf")
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: unsupported glTF file format: {}", file_path.string()));

	GltfDocument document;
	std::string error;
	tinygltf::TinyGLTF loader;
	loader.SetImageLoader(load_gltf_image_data, nullptr);
	const bool loaded = is_glb
		? loader.LoadBinaryFromFile(&document.model, &error, &document.warning, file_path.string())
		: loader.LoadASCIIFromFile(&document.model, &error, &document.warning, file_path.string());
	if (!loaded)
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: failed to load '{}': {}", file_path.string(), error));
	return document;
}

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

std::unordered_map<std::string, size_t> require_named_target_bones(const std::vector<Bone>& bones)
{
	std::unordered_map<std::string, size_t> indices;
	for (size_t index = 0; index < bones.size(); ++index)
	{
		if (bones[index].name.empty())
			throw ResourceLoadError("ResourceLoader: target skeleton contains an unnamed bone");
		if (!indices.emplace(bones[index].name, index).second)
			throw ResourceLoadError(fmt::format(
				"ResourceLoader: target skeleton contains duplicate bone name '{}'", bones[index].name));
		if (bones[index].parent_node != Bone::NO_PARENT && bones[index].parent_node >= bones.size())
			throw ResourceLoadError("ResourceLoader: target skeleton contains an invalid parent index");
	}
	return indices;
}

std::optional<std::vector<size_t>> exact_joint_mapping(
	const tinygltf::Model& model,
	const tinygltf::Skin& skin,
	const std::vector<Bone>& target_bones,
	const std::unordered_map<std::string, size_t>& target_by_name,
	const std::vector<int>& node_parents)
{
	if (skin.joints.size() != target_bones.size())
		return std::nullopt;

	std::unordered_map<int, size_t> source_joint_indices;
	std::unordered_set<std::string> source_names;
	std::vector<size_t> mapping;
	mapping.reserve(skin.joints.size());
	for (size_t source_index = 0; source_index < skin.joints.size(); ++source_index)
	{
		const int node_index = skin.joints[source_index];
		if (node_index < 0 || node_index >= static_cast<int>(model.nodes.size()))
			return std::nullopt;
		const auto& name = model.nodes[node_index].name;
		const auto target = target_by_name.find(name);
		if (name.empty() || target == target_by_name.end() || !source_names.insert(name).second)
			return std::nullopt;
		source_joint_indices.emplace(node_index, source_index);
		mapping.push_back(target->second);
	}

	for (size_t source_index = 0; source_index < skin.joints.size(); ++source_index)
	{
		int parent = node_parents.at(skin.joints[source_index]);
		while (parent >= 0 && !source_joint_indices.contains(parent))
			parent = node_parents.at(parent);

		const auto target_index = mapping[source_index];
		const auto target_parent = target_bones[target_index].parent_node;
		const std::string source_parent_name = parent < 0 ? std::string{} : model.nodes[parent].name;
		const std::string target_parent_name = target_parent == Bone::NO_PARENT
			? std::string{} : target_bones[target_parent].name;
		if (source_parent_name != target_parent_name)
			return std::nullopt;
	}
	return mapping;
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
		throw ResourceLoadError("ResourceLoader: skin has no joints");

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
			throw ResourceLoadError("ResourceLoader: inverse bind matrices must be float mat4 values matching joints");
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
		throw ResourceLoadError(message);
	result.warnings.push_back({ std::move(message) });
}
}

ResourceLoader::LoadedModel ResourceLoader::load_model(
	ECS& ecs,
	const std::string_view filename,
	const LoadOptions& options)
{
	try
	{
	const auto file_path = resolve_resource_filename(filename, Utility::get_model);
	const std::string provenance_source(filename);
	auto document = load_gltf_document(file_path);
	auto& model = document.model;
	if (model.scenes.empty())
		throw ResourceLoadError("ResourceLoader::load_model: model contains no scenes");

	const int scene_index = options.scene_index.value_or(model.defaultScene >= 0 ? model.defaultScene : 0);
	if (scene_index < 0 || scene_index >= static_cast<int>(model.scenes.size()))
		throw ResourceLoadError("ResourceLoader::load_model: requested scene is out of range");

	LoadedModel result;
	if (!document.warning.empty())
		result.warnings.push_back({ document.warning });
	if (!model.animations.empty())
		add_warning(result, options,
			"ResourceLoader::load_model: animations were ignored; use ResourceLoader::load_animations to load them explicitly");
	global_resource_loader.gltf_material_to_material.clear();
	for (size_t material_index = 0; material_index < model.materials.size(); ++material_index)
	{
		const auto& normal_texture = model.materials[material_index].normalTexture;
		if (normal_texture.index >= 0 && std::abs(normal_texture.scale - 1.0) > 0.00001)
			add_warning(result, options, fmt::format(
				"ResourceLoader: material {} normalTexture.scale={} is not supported; using 1.0",
				material_index, normal_texture.scale));
	}

	std::unordered_map<int, std::vector<Bone>> skins;
	for (const NodeInstance& instance : collect_mesh_nodes(model, model.scenes[scene_index]))
	{
		const auto& node = model.nodes.at(instance.node_index);
		if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
			throw ResourceLoadError("ResourceLoader::load_model: node references an invalid mesh");

		LoadedMesh loaded_mesh;
		loaded_mesh.name = node.name.empty() ? model.meshes[node.mesh].name : node.name;
		loaded_mesh.transform.set_mat4(instance.world_transform);
		loaded_mesh.source_node = instance.node_index;
		loaded_mesh.source_skin = node.skin;

		std::optional<SkeletonID> skeleton;
		if (node.skin >= 0)
		{
			if (node.skin >= static_cast<int>(model.skins.size()))
				throw ResourceLoadError("ResourceLoader::load_model: node references an invalid skin");
			auto it = skins.find(node.skin);
			if (it == skins.end())
				it = skins.emplace(node.skin, load_bones(model, node.skin)).first;
			skeleton = ecs.add_skeleton(it->second);
			ResourceProvenance::register_skeleton(*skeleton, {
				.source = provenance_source, .scene = scene_index, .node = instance.node_index, .skin = node.skin });
			loaded_mesh.skeleton_id = skeleton;
		}

		for (size_t primitive_index = 0; primitive_index < model.meshes[node.mesh].primitives.size(); ++primitive_index)
		{
			const auto& primitive = model.meshes[node.mesh].primitives[primitive_index];
			const auto position_it = primitive.attributes.find("POSITION");
			if (position_it == primitive.attributes.end())
				throw ResourceLoadError("ResourceLoader: primitive is missing POSITION");
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
				throw ResourceLoadError("ResourceLoader: primitive is missing NORMAL");
			if (positions.size() != normals.size())
				throw ResourceLoadError("ResourceLoader: POSITION and NORMAL counts differ");

			const auto loaded_material = global_resource_loader.load_material(primitive, model);
			const bool has_texcoords = GltfImport::has_attribute(primitive, "TEXCOORD_0");
			const bool textured = [&]
			{
				if (primitive.material < 0)
					return false;
				const auto& material = model.materials.at(primitive.material);
				if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
					return true;
				const auto extension = material.extensions.find("KHR_materials_specular");
				return extension != material.extensions.end() && extension->second.IsObject()
					&& (extension->second.Has("specularTexture")
						|| extension->second.Has("specularColorTexture"));
			}();
			const bool normal_mapped = primitive.material >= 0 &&
				model.materials.at(primitive.material).normalTexture.index >= 0;
			if (normal_mapped && !has_texcoords)
				throw ResourceLoadError("ResourceLoader: normal-mapped primitive is missing TEXCOORD_0");

			auto texcoords = has_texcoords
				? GltfImport::read_vec2(model, primitive.attributes.at("TEXCOORD_0"))
				: std::vector<glm::vec2>(positions.size(), glm::vec2(0.0f));
			if (texcoords.size() != positions.size())
				throw ResourceLoadError("ResourceLoader: POSITION and TEXCOORD_0 counts differ");

			std::vector<glm::vec4> tangents(positions.size(), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			std::optional<GltfImport::TangentRemap> tangent_remap;
			if (normal_mapped)
			{
				if (GltfImport::has_attribute(primitive, "TANGENT"))
				{
					tangents = GltfImport::read_vec4(model, primitive.attributes.at("TANGENT"));
					if (tangents.size() != positions.size())
						throw ResourceLoadError("ResourceLoader: POSITION and TANGENT counts differ");
					const bool tangents_valid = std::all_of(
						tangents.begin(), tangents.end(), [](const glm::vec4& tangent)
					{
						const glm::vec3 tangent_xyz(tangent);
						return std::isfinite(tangent.x) && std::isfinite(tangent.y) && std::isfinite(tangent.z) &&
							std::isfinite(tangent.w) && glm::length(tangent_xyz) >= 0.00001f &&
							std::abs(std::abs(tangent.w) - 1.0f) <= 0.00001f;
					});
					if (!tangents_valid)
					{
						if (!options.generate_missing_tangents)
							throw ResourceLoadError("ResourceLoader: primitive contains an invalid TANGENT");
						add_warning(result, options, "ResourceLoader: regenerated invalid tangents");
						tangent_remap = GltfImport::generate_tangents(positions, normals, texcoords, indices);
					}
					else
					{
						for (auto& tangent : tangents)
						{
							const glm::vec3 tangent_xyz(tangent);
							tangent = glm::vec4(glm::normalize(tangent_xyz), tangent.w < 0.0f ? -1.0f : 1.0f);
						}
					}
				}
				else if (!options.generate_missing_tangents)
					throw ResourceLoadError("ResourceLoader: normal-mapped primitive is missing TANGENT");
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
				const bool has_additional_joint_set = std::any_of(
					primitive.attributes.begin(), primitive.attributes.end(), [](const auto& attribute)
				{
					const auto& semantic = attribute.first;
					return (semantic.starts_with("JOINTS_") && semantic != "JOINTS_0")
						|| (semantic.starts_with("WEIGHTS_") && semantic != "WEIGHTS_0");
				});
				if (has_additional_joint_set)
					throw ResourceLoadError(
						"ResourceLoader: skinned primitive exceeds the maximum of 4 bone influences per vertex");
				if (!GltfImport::has_attribute(primitive, "JOINTS_0") || !GltfImport::has_attribute(primitive, "WEIGHTS_0"))
					throw ResourceLoadError("ResourceLoader: skinned primitive is missing JOINTS_0 or WEIGHTS_0");
				auto joints = GltfImport::read_vec4(model, primitive.attributes.at("JOINTS_0"), false);
				auto weights = GltfImport::read_vec4(model, primitive.attributes.at("WEIGHTS_0"));
				if (texcoords.size() != positions.size() || joints.size() != positions.size() || weights.size() != positions.size())
					throw ResourceLoadError("ResourceLoader: skinned vertex attribute counts differ");
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
				renderable.pipeline_render_type = textured
					? ERenderType::SKINNED : ERenderType::SKINNED_COLOR;
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
			ResourceProvenance::register_mesh(renderable.mesh_id, {
				.source = provenance_source, .scene = scene_index, .node = instance.node_index,
				.primitive = static_cast<int>(primitive_index), .material = primitive.material, .skin = node.skin });
			for (const auto material_id : loaded_material.ids)
				ResourceProvenance::register_material(material_id, {
					.source = provenance_source, .scene = scene_index, .node = instance.node_index,
					.primitive = static_cast<int>(primitive_index), .material = primitive.material, .skin = node.skin });
			renderable.material_ids = loaded_material.ids;
			renderable.alpha_mode = loaded_material.alpha_mode;
			renderable.alpha_cutoff = loaded_material.alpha_cutoff;
			renderable.opacity = loaded_material.opacity;
			loaded_mesh.renderables.push_back(renderable);
		}
		result.meshes.push_back(std::move(loaded_mesh));
	}
	if (result.meshes.empty())
		add_warning(result, options, "ResourceLoader: selected scene contains no mesh nodes");
	return result;
	}
	catch (const std::out_of_range& error)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: resource contains an invalid index: {}", error.what()));
	}
}

ResourceLoader::LoadedModel ResourceLoader::load_model(ECS& ecs, const std::string_view filename)
{
	LoadOptions options;
	options.generate_missing_tangents = true;
	return load_model(ecs, filename, options);
}

ResourceLoader::LoadedAnimations ResourceLoader::load_animations(
	ECS& ecs,
	const std::string_view filename,
	const SkeletonID target_skeleton)
{
	try
	{
		const auto file_path = resolve_resource_filename(filename, Utility::get_animation);
		const std::string provenance_source(filename);

		auto document = load_gltf_document(file_path);
		const auto& model = document.model;
		if (model.animations.empty())
			throw ResourceLoadError("ResourceLoader: animation file contains no animations");
		if (model.skins.empty())
			throw ResourceLoadError("ResourceLoader: animation file contains no skins");

		const auto& target_bones = ecs.get_skeletal_component(target_skeleton).get_bones();
		const auto target_by_name = require_named_target_bones(target_bones);
		const auto node_parents = get_parent_nodes(model);

		std::vector<std::pair<int, std::vector<size_t>>> compatible_skins;
		for (int skin_index = 0; skin_index < static_cast<int>(model.skins.size()); ++skin_index)
		{
			if (auto mapping = exact_joint_mapping(
				model, model.skins[skin_index], target_bones, target_by_name, node_parents))
			{
				compatible_skins.emplace_back(skin_index, std::move(*mapping));
			}
		}
		if (compatible_skins.empty())
			throw ResourceLoadError("ResourceLoader: animation file has no skin compatible with the target skeleton");
		if (compatible_skins.size() > 1)
			throw ResourceLoadError("ResourceLoader: animation file has multiple skins compatible with the target skeleton");

		auto imported = import_animations(
			model, target_bones, compatible_skins.front().first, compatible_skins.front().second);
		if (imported.empty())
			throw ResourceLoadError("ResourceLoader: animation file contains no clips for the compatible skin");
		LoadedAnimations result;
		if (!document.warning.empty())
			result.warnings.push_back({ document.warning });
		result.animations.reserve(imported.size());
		const auto signature = make_skeletal_rig_signature(target_bones);
		for (auto& animation : imported)
		{
			const auto animation_id = ecs.add_skeletal_animation(
				animation.name,
				std::move(animation.bone_animations),
				signature,
				provenance_source);
			ResourceProvenance::register_animation(animation_id, {
				.source = provenance_source, .skin = compatible_skins.front().first,
				.animation = animation.source_index });
			result.animations.push_back(animation_id);
		}
		return result;
	}
	catch (const std::out_of_range& error)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: resource contains an invalid index: {}", error.what()));
	}
}
