#include "resource_loader.hpp"
#include "resource_loader_mesh.ipp"
#include "resource_loader_animation.ipp"
#include "resource_loader_material.ipp"
#include "entity_component_system/ecs.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
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

MaterialHandle ResourceLoader::fetch_texture(
	MaterialSystem& materials,
	const std::string_view logical_resource_name,
	const ETextureSemantic semantic)
{
	if (semantic == ETextureSemantic::COUNT)
		throw ResourceLoadError("ResourceLoader::fetch_texture: invalid texture semantic");
	const auto resolved_file_path = resolve_resource_filename(logical_resource_name, Utility::get_texture);
	auto owner = global_resource_loader.load_texture(
		materials, resolved_file_path, logical_resource_name, semantic);
	ResourceProvenance::register_material(owner->get_id(), {
		.kind = EExternalResourceKind::Texture,
		.source = std::string(logical_resource_name),
		.texture_semantic = static_cast<int>(semantic),
	});
	return owner;
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
		transform.set_mat4(glm::make_mat4(node.matrix.data()));
	else
	{
		if (!node.translation.empty())
			transform.set_pos({ node.translation[0], node.translation[1], node.translation[2] });
		if (!node.rotation.empty())
			transform.set_orient({
				static_cast<float>(node.rotation[3]),
				static_cast<float>(node.rotation[0]),
				static_cast<float>(node.rotation[1]),
				static_cast<float>(node.rotation[2])
			});
		if (!node.scale.empty())
			transform.set_scale({ node.scale[0], node.scale[1], node.scale[2] });
	}
	return Maths::Transform(GltfImport::to_krisp_basis(transform.get_mat4()));
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
			bones[joint].inverse_bind_pose.set_mat4(GltfImport::to_krisp_basis(matrix));
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
		// Renderable::local_transform and must not become part of the root bone. Helper
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
	const auto node_instances = collect_mesh_nodes(model, model.scenes[scene_index]);
	std::unordered_set<int> used_materials;
	for (const auto& instance : node_instances)
	{
		const auto& node = model.nodes.at(instance.node_index);
		if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
			throw ResourceLoadError("ResourceLoader::load_model: node references an invalid mesh");
		for (const auto& primitive : model.meshes[node.mesh].primitives)
			if (primitive.material >= 0)
			{
				if (primitive.material >= static_cast<int>(model.materials.size()))
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: primitive references invalid material {}", primitive.material));
				used_materials.insert(primitive.material);
			}
	}
	validate_gltf_materials(model, used_materials, result.warnings, options.strict);
	if (!model.animations.empty())
		add_warning(result, options,
			"ResourceLoader::load_model: animations were ignored; use ResourceLoader::load_animations to load them explicitly");
	global_resource_loader.gltf_material_to_material.clear();
	global_resource_loader.gltf_image_to_material.clear();

	std::unordered_map<int, std::vector<Bone>> skins;
	for (const NodeInstance& instance : node_instances)
	{
		const auto& node = model.nodes.at(instance.node_index);
		if (node.mesh < 0 || node.mesh >= static_cast<int>(model.meshes.size()))
			throw ResourceLoadError("ResourceLoader::load_model: node references an invalid mesh");

		LoadedMesh loaded_mesh;
		loaded_mesh.name = node.name.empty() ? model.meshes[node.mesh].name : node.name;
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
			auto positions = GltfImport::read_vec3(model, position_it->second);
			for (auto& position : positions)
				position = GltfImport::to_krisp_basis(position);
			auto indices = GltfImport::read_indices(model, primitive, positions.size());
			if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
				add_warning(result, options, "ResourceLoader: converted a non-triangle primitive to triangles");
			indices = GltfImport::triangles_from(primitive, std::move(indices), options.allow_non_triangle_primitives);
			GltfImport::reverse_triangle_winding(indices);

			std::vector<glm::vec3> normals;
			if (GltfImport::has_attribute(primitive, "NORMAL"))
			{
				normals = GltfImport::read_vec3(model, primitive.attributes.at("NORMAL"));
				for (auto& normal : normals)
					normal = glm::normalize(GltfImport::to_krisp_basis(normal));
			}
			else if (options.generate_missing_normals)
			{
				add_warning(result, options, "ResourceLoader: generated missing normals");
				normals = GltfImport::generate_normals(positions, indices);
			}
			else
				throw ResourceLoadError("ResourceLoader: primitive is missing NORMAL");
			if (positions.size() != normals.size())
				throw ResourceLoadError("ResourceLoader: POSITION and NORMAL counts differ");

			std::vector<MaterialHandle> material_owners;
			const auto loaded_material = global_resource_loader.load_material(
				ecs.get_material_system(), primitive, model, material_owners);
			const auto* pbr_material = dynamic_cast<const PbrMaterial*>(
				&material_owners.front()->get());
			if (!pbr_material)
				throw ResourceLoadError("ResourceLoader: glTF material is not PBR");
			const bool textured = pbr_material->has_textures();
			const bool normal_mapped = pbr_material->textures.normal.has_value();
			std::vector<glm::vec2> texcoords;
			std::vector<glm::vec4> tangents;
			std::optional<GltfImport::TangentRemap> generated_tangents;
			if (textured)
			{
				if (!GltfImport::has_attribute(primitive, "TEXCOORD_0"))
					throw ResourceLoadError(
						"ResourceLoader: textured primitive is missing TEXCOORD_0");
				texcoords = GltfImport::read_vec2(model, primitive.attributes.at("TEXCOORD_0"));
				if (texcoords.size() != positions.size())
					throw ResourceLoadError(
						"ResourceLoader: POSITION and TEXCOORD_0 counts differ");
				if (GltfImport::has_attribute(primitive, "TANGENT"))
				{
					GltfImport::AccessorReader tangent_reader(
						model, primitive.attributes.at("TANGENT"));
					if (tangent_reader.accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
						throw ResourceLoadError("ResourceLoader: TANGENT must use float components");
					tangents = GltfImport::read_vec4(model, primitive.attributes.at("TANGENT"));
					if (tangents.size() != positions.size())
						throw ResourceLoadError("ResourceLoader: POSITION and TANGENT counts differ");
					bool tangents_are_valid = true;
					for (size_t tangent_index = 0; tangent_index < tangents.size(); ++tangent_index)
					{
						auto& tangent = tangents[tangent_index];
						const float length = glm::length(glm::vec3(tangent));
						if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y)
							|| !std::isfinite(tangent.z) || !std::isfinite(tangent.w)
							|| std::abs(length - 1.0f) > 0.001f
							|| glm::length(glm::cross(glm::vec3(tangent), normals[tangent_index]))
								< 0.00001f
							|| std::abs(std::abs(tangent.w) - 1.0f) > 0.001f)
						{
							tangents_are_valid = false;
							break;
						}
						tangent = GltfImport::tangent_to_krisp_basis(tangent);
					}
					if (!tangents_are_valid)
					{
						if (!options.regenerate_invalid_tangents)
							throw ResourceLoadError("ResourceLoader: TANGENT contains invalid values");
						add_warning(result, options, "ResourceLoader: regenerated invalid tangents");
						generated_tangents = GltfImport::generate_tangents(
							positions, normals, texcoords, indices);
					}
				}
				else if (normal_mapped)
				{
					if (!options.generate_missing_tangents)
						throw ResourceLoadError(
							"ResourceLoader: normal-mapped primitive is missing TANGENT");
					add_warning(result, options, "ResourceLoader: generated missing tangents");
					generated_tangents = GltfImport::generate_tangents(
						positions, normals, texcoords, indices);
				}
				else
				{
					tangents.assign(positions.size(), glm::vec4(0.0f));
				}
			}
			Renderable renderable;
			renderable.name = loaded_mesh.name;
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
				if (joints.size() != positions.size() || weights.size() != positions.size())
					throw ResourceLoadError("ResourceLoader: skinned vertex attribute counts differ");
				if (textured)
				{
					if (generated_tangents)
						mesh = std::make_unique<SkinnedMesh>(load_skinned_vertices(
							positions, normals, texcoords, *generated_tangents, joints, weights),
							std::move(generated_tangents->indices));
					else
						mesh = std::make_unique<SkinnedMesh>(load_skinned_vertices(
							positions, normals, texcoords, tangents, joints, weights),
							std::move(indices));
					renderable.pipeline_render_type = ERenderType::SKINNED;
				}
				else
				{
					const std::vector<glm::vec2> empty_texcoords(
						positions.size(), glm::vec2(0.0f));
					const std::vector<glm::vec4> empty_tangents(
						positions.size(), glm::vec4(0.0f));
					mesh = std::make_unique<SkinnedMesh>(load_skinned_vertices(
						positions, normals, empty_texcoords, empty_tangents, joints, weights),
						std::move(indices));
					renderable.pipeline_render_type = ERenderType::SKINNED_COLOR;
				}
			}
			else
			{
				if (textured)
				{
					if (generated_tangents)
						mesh = std::make_unique<TexMesh>(load_tex_vertices(
							positions, normals, texcoords, *generated_tangents),
							std::move(generated_tangents->indices));
					else
						mesh = std::make_unique<TexMesh>(load_tex_vertices(
							positions, normals, texcoords, tangents), std::move(indices));
					renderable.pipeline_render_type = ERenderType::STANDARD;
				}
				else
				{
					mesh = std::make_unique<ColorMesh>(
						load_color_vertices(positions, normals), std::move(indices));
					renderable.pipeline_render_type = ERenderType::COLOR;
				}
			}
			renderable.mesh_owner = ecs.get_mesh_system().add(std::move(mesh));
			const auto mesh_id = renderable.mesh_owner->get_id();
			ResourceProvenance::register_mesh(mesh_id, {
				.source = provenance_source, .scene = scene_index, .node = instance.node_index,
				.primitive = static_cast<int>(primitive_index), .material = primitive.material, .skin = node.skin });
			ResourceProvenance::register_material(loaded_material.ids.front(), {
				.source = provenance_source, .scene = scene_index, .material = primitive.material });
			for (const auto& [material_id, image_index] : loaded_material.image_sources)
			{
				const auto* texture = dynamic_cast<const TextureMaterial*>(
					&ecs.get_material_system().get(material_id));
				if (!texture)
					throw ResourceLoadError("ResourceLoader: cached glTF texture has the wrong type");
				ResourceProvenance::register_material(material_id, {
					.source = provenance_source, .scene = scene_index,
					.image = image_index,
					.texture_semantic = static_cast<int>(texture->semantic) });
			}
			renderable.material_owners = std::move(material_owners);
			renderable.local_transform.set_mat4(instance.world_transform);
			loaded_mesh.renderables.push_back(std::move(renderable));
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
