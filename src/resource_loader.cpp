#include "resource_loader.hpp"
#include "maths.hpp"
#include "objects/object.hpp"
#include "analytics.hpp"
#include "entity_component_system/ecs.hpp"
#include "entity_component_system/skeletal.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/mesh.hpp"
#include "renderable/material_factory.hpp"

#include <stb_image.h>
#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/core.h>

#include <iostream>
#include <map>
#include <unordered_map>
#include <filesystem>


ResourceLoader ResourceLoader::global_resource_loader;

struct RawTextureDataSTB : public TextureData
{
	RawTextureDataSTB(stbi_uc* data) : data(reinterpret_cast<std::byte*>(data)) {}

	virtual ~RawTextureDataSTB() override
	{
		stbi_image_free(data); 
	}

	virtual std::byte* get() override 
	{ 
		return data; 
	}

private:
	std::byte* data;
};

struct RawTextureDataGLTF : public TextureData
{
	RawTextureDataGLTF(std::vector<unsigned char>&& data) :
		data(std::move(data))
	{
	}

	virtual std::byte* get() override 
	{ 
		return reinterpret_cast<std::byte*>(data.data()); 
	}

private:
	std::vector<unsigned char> data;
};

MaterialID ResourceLoader::fetch_texture(const std::string_view file)
{
	if (global_resource_loader.texture_name_to_mat_id.contains(file.data()))
	{
		return global_resource_loader.texture_name_to_mat_id[file.data()];
	}

	return global_resource_loader.load_texture(file);
}

std::vector<uint32_t> load_indices(const tinygltf::Accessor& index_accessor, 
								   const tinygltf::BufferView& index_buffer_view, 
								   const tinygltf::Buffer& index_buffer)
{
	std::vector<uint32_t> indices(index_accessor.count);
	if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		const auto* index_data = 
			reinterpret_cast<const uint16_t*>(&index_buffer.data[index_accessor.byteOffset + index_buffer_view.byteOffset]);
		for (size_t i = 0; i < index_accessor.count; ++i)
		{
			indices[i] = index_data[i];
		}
	} else
	{
		const auto* index_data = 
			reinterpret_cast<const uint32_t*>(&index_buffer.data[index_accessor.byteOffset + index_buffer_view.byteOffset]);
		std::memcpy(indices.data(), index_data, indices.size() * sizeof(indices[0]));
	}

	return indices;
}

template<typename dst_t, typename src_t>
void copy_data(const src_t* src, dst_t& dst, const uint32_t idx, const int component_type = TINYGLTF_COMPONENT_TYPE_FLOAT)
{
	const size_t start_offset = dst_t::length() * idx;
	const size_t end_offset = start_offset + dst_t::length();
	switch (component_type) // TODO: this might be expensive
	{
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
		{
			const auto* src_ptr = reinterpret_cast<const float*>(src);
			std::copy(src_ptr+start_offset, src_ptr+end_offset, glm::value_ptr(dst));
			break;
		}
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			const auto* src_ptr = reinterpret_cast<const uint8_t*>(src);
			std::copy(src_ptr+start_offset, src_ptr+end_offset, glm::value_ptr(dst));
			break;
		}
	default:
		throw std::runtime_error("ResourceLoader: unsupported component type");
	}
}

template<typename VerticesType>
VerticesType load_vertices(const tinygltf::Model& model, tinygltf::Primitive& primitive);

template<>
ColorVertices load_vertices<ColorVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	if (primitive.attributes.find("POSITION") == primitive.attributes.end() || 
		primitive.attributes.find("NORMAL") == primitive.attributes.end())
	{
		throw std::runtime_error("ResourceLoader: missing attributes");
	}

	const auto& pos_accessor = model.accessors[primitive.attributes["POSITION"]];
	const auto& norm_accessor = model.accessors[primitive.attributes["NORMAL"]];

	if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 positions are supported");
	}

	if (norm_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || norm_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 normals are supported");
	}

	const auto& pos_buffer_view = model.bufferViews[pos_accessor.bufferView];
	const auto& norm_buffer_view = model.bufferViews[norm_accessor.bufferView];

	const auto& pos_buffer = model.buffers[pos_buffer_view.buffer];
	const auto& norm_buffer = model.buffers[norm_buffer_view.buffer];

	const auto* pos_data = reinterpret_cast<const float*>(&pos_buffer.data[pos_accessor.byteOffset + pos_buffer_view.byteOffset]);
	const auto* norm_data = reinterpret_cast<const float*>(&norm_buffer.data[norm_accessor.byteOffset + norm_buffer_view.byteOffset]);

	// convert data to our mesh format
	ColorVertices vertices(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
	}

	return vertices;
}

template<>
TexVertices load_vertices<TexVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	if (primitive.attributes.find("POSITION") == primitive.attributes.end() || 
		primitive.attributes.find("NORMAL") == primitive.attributes.end() || 
		primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
	{
		throw std::runtime_error("ResourceLoader: missing attributes");
	}

	const auto& pos_accessor = model.accessors[primitive.attributes["POSITION"]];
	const auto& norm_accessor = model.accessors[primitive.attributes["NORMAL"]];
	const auto& tex_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];

	if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 positions are supported");
	}

	if (norm_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || norm_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 normals are supported");
	}

	if (tex_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || tex_accessor.type != TINYGLTF_TYPE_VEC2)
	{
		throw std::runtime_error("ResourceLoader: only float vec2 texcoords are supported");
	}

	const auto& pos_buffer_view = model.bufferViews[pos_accessor.bufferView];
	const auto& norm_buffer_view = model.bufferViews[norm_accessor.bufferView];
	const auto& tex_buffer_view = model.bufferViews[tex_accessor.bufferView];

	const auto& pos_buffer = model.buffers[pos_buffer_view.buffer];
	const auto& norm_buffer = model.buffers[norm_buffer_view.buffer];
	const auto& tex_buffer = model.buffers[tex_buffer_view.buffer];

	const auto* pos_data = reinterpret_cast<const float*>(&pos_buffer.data[pos_accessor.byteOffset + pos_buffer_view.byteOffset]);
	const auto* norm_data = reinterpret_cast<const float*>(&norm_buffer.data[norm_accessor.byteOffset + norm_buffer_view.byteOffset]);
	const auto* tex_data = reinterpret_cast<const float*>(&tex_buffer.data[tex_accessor.byteOffset + tex_buffer_view.byteOffset]);

	// convert data to our mesh format
	TexVertices vertices(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
		copy_data(tex_data, vertex.texCoord, i);
	}

	return vertices;
}

template<>
SkinnedVertices load_vertices<SkinnedVertices>(const tinygltf::Model& model, tinygltf::Primitive& primitive)
{
	if (primitive.attributes.find("POSITION") == primitive.attributes.end() ||
		primitive.attributes.find("NORMAL") == primitive.attributes.end() ||
		primitive.attributes.find("JOINTS_0") == primitive.attributes.end() ||
		primitive.attributes.find("WEIGHTS_0") == primitive.attributes.end() ||
		primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
	{
		throw std::runtime_error("ResourceLoader: missing attributes");
	}

	if (model.skins.size() != 1)
	{
		throw std::runtime_error("ResourceLoader: only one skin is supported");
	}

	const auto& pos_accessor = model.accessors[primitive.attributes["POSITION"]];
	const auto& norm_accessor = model.accessors[primitive.attributes["NORMAL"]];
	const auto& joint_accessor = model.accessors[primitive.attributes["JOINTS_0"]];
	const auto& weight_accessor = model.accessors[primitive.attributes["WEIGHTS_0"]];
	const auto& tex_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];

	if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 positions are supported");
	}

	if (norm_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || norm_accessor.type != TINYGLTF_TYPE_VEC3)
	{
		throw std::runtime_error("ResourceLoader: only float vec3 normals are supported");
	}

	static const std::unordered_set<int> SUPPORTED_TYPES = { 
		TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, 
		TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT 
	};
	if (!SUPPORTED_TYPES.contains(joint_accessor.componentType) || joint_accessor.type != TINYGLTF_TYPE_VEC4)
	{
		throw std::runtime_error("ResourceLoader: only unsigned byte/short vec4 joints are supported");
	}

	if (weight_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || weight_accessor.type != TINYGLTF_TYPE_VEC4)
	{
		throw std::runtime_error("ResourceLoader: only float vec4 weights are supported");
	}

	if (tex_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || tex_accessor.type != TINYGLTF_TYPE_VEC2)
	{
		throw std::runtime_error("ResourceLoader: only float vec2 texcoords are supported");
	}

	const auto& pos_buffer_view = model.bufferViews[pos_accessor.bufferView];
	const auto& norm_buffer_view = model.bufferViews[norm_accessor.bufferView];
	const auto& joint_buffer_view = model.bufferViews[joint_accessor.bufferView];
	const auto& weight_buffer_view = model.bufferViews[weight_accessor.bufferView];
	const auto& tex_buffer_view = model.bufferViews[tex_accessor.bufferView];

	const auto& pos_buffer = model.buffers[pos_buffer_view.buffer];
	const auto& norm_buffer = model.buffers[norm_buffer_view.buffer];
	const auto& joint_buffer = model.buffers[joint_buffer_view.buffer];
	const auto& weight_buffer = model.buffers[weight_buffer_view.buffer];
	const auto& tex_buffer = model.buffers[tex_buffer_view.buffer];

	const auto* pos_data = &pos_buffer.data[pos_accessor.byteOffset + pos_buffer_view.byteOffset];
	const auto* norm_data = &norm_buffer.data[norm_accessor.byteOffset + norm_buffer_view.byteOffset];
	const auto* joint_data = &joint_buffer.data[joint_accessor.byteOffset + joint_buffer_view.byteOffset];
	const auto* weight_data = &weight_buffer.data[weight_accessor.byteOffset + weight_buffer_view.byteOffset];
	const auto* tex_data = &tex_buffer.data[tex_accessor.byteOffset + tex_buffer_view.byteOffset];

	SkinnedVertices vertices(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		auto& vertex = vertices[i];
		copy_data(pos_data, vertex.pos, i);
		copy_data(norm_data, vertex.normal, i);
		copy_data(tex_data, vertex.texCoord, i);
		copy_data(joint_data, vertex.bone_ids, i, joint_accessor.componentType);
		copy_data(weight_data, vertex.bone_weights, i);
	}

	return vertices;
}

MaterialID ResourceLoader::load_material(const tinygltf::Primitive& primitive, tinygltf::Model& model)
{
	if (primitive.material >= 0) // if it contains a material
	{
		const auto& mat = model.materials[primitive.material];
		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		if (color_texture.index >= 0)
		{
			// has color texture
			tinygltf::Image& image = model.images[color_texture.index];
			TextureMaterial new_material;
			new_material.width = image.width;
			new_material.height = image.height;
			new_material.channels = image.component;
			new_material.data = std::make_unique<RawTextureDataGLTF>(std::move(image.image));

			return MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(new_material)));
		} else
		{
			ColorMaterial new_material;
			new_material.data.diffuse = glm::vec3(
				mat.pbrMetallicRoughness.baseColorFactor[0],
				mat.pbrMetallicRoughness.baseColorFactor[1],
				mat.pbrMetallicRoughness.baseColorFactor[2]);
			new_material.data.ambient = new_material.data.diffuse;
			new_material.data.specular = (new_material.data.specular + new_material.data.diffuse)/2.0f;
			new_material.data.shininess = 1 - mat.pbrMetallicRoughness.roughnessFactor;

			return MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(new_material)));
		}
	}

	return MaterialFactory::fetch_preset(EMaterialPreset::PLASTIC);
}

static std::vector<Bone> load_bones(const tinygltf::Model& model)
{
	if (model.skins.size() != 1)
	{
		throw std::runtime_error("ResourceLoader: only one skin is supported");
	}
	const auto& skin = model.skins[0];

	const auto& inverse_bind_matrices_accessor = model.accessors[skin.inverseBindMatrices];

	if (inverse_bind_matrices_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || inverse_bind_matrices_accessor.type != TINYGLTF_TYPE_MAT4)
	{
		throw std::runtime_error("ResourceLoader: only float mat4 inverse bind matrices are supported");
	}

	const auto& inverse_bind_matrices_buffer_view = model.bufferViews[inverse_bind_matrices_accessor.bufferView];
	const auto& inverse_bind_matrices_buffer = model.buffers[inverse_bind_matrices_buffer_view.buffer];
	const auto* inverse_bind_matrices_data = reinterpret_cast<const float*>(
		&inverse_bind_matrices_buffer.data[inverse_bind_matrices_accessor.byteOffset + inverse_bind_matrices_buffer_view.byteOffset]);

	const std::map<int, int> node_to_joint = [&skin]()
	{
		std::map<int, int> retval;
		for (int i = 0; i < skin.joints.size(); i++)
		{
			retval[skin.joints[i]] = i;
		}
		return retval;
	}();
	std::vector<Bone> bones(skin.joints.size());
	for (int i = 0; i < bones.size(); i++)
	{
		const auto joint_idx = skin.joints[i];
		const auto& node = model.nodes[joint_idx];
		Bone& bone = bones[i];

		bone.name = node.name;
		bone.inverse_bind_pose.set_mat4(glm::make_mat4(&inverse_bind_matrices_data[i*sizeof(glm::mat4)/sizeof(float)]));

		for (int child : node.children)
		{
			bones[node_to_joint.at(child)].parent_node = i;
		}

		if (!node.translation.empty())
		{
			bone.relative_transform.set_pos(glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
		}

		if (!node.rotation.empty())
		{
			bone.relative_transform.set_orient(glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));
		}

		if (!node.scale.empty())
		{
			bone.relative_transform.set_scale(glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
		}
		
		bone.original_transform = bone.relative_transform;
	}

	return bones;
}

struct TempKeyFrame
{
	std::optional<glm::vec3> pos;
	std::optional<glm::quat> orient;
	std::optional<glm::vec3> scale;
};

using TKFS = std::map<float, TempKeyFrame>;

std::vector<std::pair<glm::vec4, float>> get_sampler_data(
	const tinygltf::Model& model, 
	const tinygltf::AnimationSampler& sampler)
{
	std::vector<std::pair<glm::vec4, float>> retval;

	if (sampler.interpolation != "LINEAR")
	{
		throw std::runtime_error("ResourceLoader: only LINEAR interpolation is supported");
	}

	if (sampler.input < 0 || sampler.input >= model.accessors.size())
	{
		throw std::runtime_error("ResourceLoader: invalid input accessor");
	}

	const auto& input_accesor = model.accessors[sampler.input];
	const auto& output_accessor = model.accessors[sampler.output];
	
	if (input_accesor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || input_accesor.type != TINYGLTF_TYPE_SCALAR)
	{
		throw std::runtime_error("ResourceLoader: only float scalar input accessors are supported");
	}

	if (output_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || 
		(output_accessor.type != TINYGLTF_TYPE_VEC3 && output_accessor.type != TINYGLTF_TYPE_VEC4))
	{
		throw std::runtime_error("ResourceLoader: only float vec3 output accessors are supported");
	}

	const auto& input_buffer_view = model.bufferViews[input_accesor.bufferView];
	const auto& input_buffer = model.buffers[input_buffer_view.buffer];
	const auto* input_data = reinterpret_cast<const float*>(&input_buffer.data[input_accesor.byteOffset + input_buffer_view.byteOffset]);
	
	const auto& output_buffer_view = model.bufferViews[output_accessor.bufferView];
	const auto& output_buffer = model.buffers[output_buffer_view.buffer];
	const auto* output_data = &output_buffer.data[output_accessor.byteOffset + output_buffer_view.byteOffset];

	retval.reserve(input_accesor.count);
	assert(input_accesor.count == output_accessor.count);

	for (int i = 0; i < input_accesor.count; ++i)
	{
		if (output_accessor.type == TINYGLTF_TYPE_VEC3)
		{
			retval.push_back({ glm::vec4(reinterpret_cast<const glm::vec3*>(output_data)[i], 1.0f), input_data[i] });
		} else
		{
			retval.push_back({ reinterpret_cast<const glm::vec4*>(output_data)[i], input_data[i] });
		}
	}

	return retval;
}

static std::vector<AnimationID> load_animations(const tinygltf::Model& model, const std::vector<Bone>& initial_bones)
{
	if (model.skins.size() != 1)
	{
		throw std::runtime_error("ResourceLoader::load_animations: only one skin is supported");
	}
	
	std::vector<AnimationID> final_animations;

	const std::map<int, int> node_to_joint = [&model] {

		std::map<int, int> retval;
		for (int i = 0; i < model.skins[0].joints.size(); i++)
		{
			retval[model.skins[0].joints[i]] = i;
		}
		return retval;
	}();

	for (const auto& animation : model.animations)
	{	
		// An animation represents a collection of BoneAnimations, each of which is a collection of keyframes
		std::vector<TKFS> bone_tkfss(model.skins[0].joints.size());

		for (const auto& channel : animation.channels)
		{
			if (channel.target_path != "rotation" && channel.target_path != "translation" && channel.target_path != "scale")
			{
				throw std::runtime_error("ResourceLoader: only rotation, translation and scale target paths are supported");
			}

			if (channel.target_node < 0 || channel.target_node >= model.nodes.size())
			{
				throw std::runtime_error("ResourceLoader: invalid target node");
			}

			const auto joint_idx = node_to_joint.at(channel.target_node);
			auto& bone_tkfs = bone_tkfss[joint_idx];

			auto sampler_data = get_sampler_data(model, animation.samplers[channel.sampler]);
			for (auto& data : sampler_data)
			{
				auto& tkf = bone_tkfs.emplace(data.second, TempKeyFrame{}).first->second;
				
				if (channel.target_path == "rotation")
				{
					tkf.orient = glm::quat(data.first.w, data.first.x, data.first.y, data.first.z);
				} else if (channel.target_path == "translation")
				{
					tkf.pos = glm::vec3(data.first);
				} else if (channel.target_path == "scale")
				{
					tkf.scale = glm::vec3(data.first);
				}
			}
		}

		// convert temporary key frames to proper key frames
		std::vector<BoneAnimation> new_bone_animations;
		for (int bone_idx = 0; bone_idx < bone_tkfss.size(); bone_idx++)
		{
			const auto& inital_bone = initial_bones.at(bone_idx);
			auto& bone_tkfs = bone_tkfss[bone_idx];
			auto& new_bone_animation = new_bone_animations.emplace_back();
			for (auto& [timestamp, tkf] : bone_tkfs)
			{
				BoneAnimation::KeyFrame new_key_frame;
				new_key_frame.animation_stage_secs = timestamp;

				if (tkf.pos.has_value())
				{
					new_key_frame.transform.set_pos(tkf.pos.value());
				} else
				{
					if (new_bone_animation.key_frames.empty())
					{
						new_key_frame.transform.set_pos(inital_bone.relative_transform.get_pos());
					} else
					{
						new_key_frame.transform.set_pos(new_bone_animation.key_frames.back().transform.get_pos());
					}
				}

				if (tkf.orient.has_value())
				{
					new_key_frame.transform.set_orient(tkf.orient.value());
				} else
				{
					if (new_bone_animation.key_frames.empty())
					{
						new_key_frame.transform.set_orient(inital_bone.relative_transform.get_orient());
					} else 
					{
						new_key_frame.transform.set_orient(new_bone_animation.key_frames.back().transform.get_orient());
					}
				}

				if (tkf.scale.has_value())
				{
					new_key_frame.transform.set_scale(tkf.scale.value());
				} else
				{
					if (new_bone_animation.key_frames.empty())
					{
						new_key_frame.transform.set_scale(inital_bone.relative_transform.get_scale());
					} else
					{
						new_key_frame.transform.set_scale(new_bone_animation.key_frames.back().transform.get_scale());
					}
				}

				new_bone_animation.animation_start_secs = std::min(new_bone_animation.animation_start_secs, timestamp);
				new_bone_animation.animation_end_secs = std::max(new_bone_animation.animation_end_secs, timestamp);
				new_bone_animation.key_frames.push_back(std::move(new_key_frame));
			}
		}

		final_animations.push_back(ECS::get().add_skeletal_animation(animation.name, std::move(new_bone_animations)));
	}

	return final_animations;
}

ResourceLoader::LoadedModel ResourceLoader::load_model(const std::string_view file)
{
	std::filesystem::path file_path(file);
	tinygltf::Model model;
	std::string err;
	std::string warn;
	tinygltf::TinyGLTF loader;
	LoadedModel retval;

	if (file_path.extension().string() == ".gltf")
	{
		if (!loader.LoadASCIIFromFile(&model, &err, &warn, file_path.string()))
		{
			throw std::runtime_error(fmt::format(
				"ResourceLoader::load_model: failed to load model: {}, err {}, warn {}", 
				file,
				err,
				warn));
		}
	} else
	{
		if (!loader.LoadBinaryFromFile(&model, &err, &warn, file_path.string()))
		{
			throw std::runtime_error(fmt::format(
				"ResourceLoader::load_model: failed to load model: {}, err {}, warn {}", 
				file,
				err,
				warn));
		}
	}

	if (model.meshes.empty())
	{
		throw std::runtime_error("ResourceLoader::load_model: no meshes found");
	}

	if (model.scenes.size() != 1 && model.scenes[0].nodes.size() != 1)
	{
		throw std::runtime_error("ResourceLoader::load_model: only one scene with one node is supported");
	}

	const auto& scene = model.scenes[0];
	const auto& root_node = model.nodes[scene.nodes[0]];
	if (!root_node.matrix.empty())
	{
		retval.onload_transform.set_mat4(glm::make_mat4(root_node.matrix.data()));
	} else
	{
		if (!root_node.translation.empty())
		{
			retval.onload_transform.set_pos(glm::vec3(root_node.translation[0], root_node.translation[1], root_node.translation[2]));
		}

		if (!root_node.rotation.empty())
		{
			retval.onload_transform.set_orient(glm::quat(root_node.rotation[3], root_node.rotation[0], root_node.rotation[1], root_node.rotation[2]));
		}

		if (!root_node.scale.empty())
		{
			retval.onload_transform.set_scale(glm::vec3(root_node.scale[0], root_node.scale[1], root_node.scale[2]));
		}
	}

	std::vector<Bone> bones;

	const bool has_bones = [&model](){ return !model.skins.empty(); }();
	if (has_bones)
	{
		bones = load_bones(model);
	}

	for (auto& mesh : model.meshes)
	{
		if (mesh.primitives.size() != 1)
		{
			throw std::runtime_error("ResourceLoader::load_model: only one primitive per mesh is supported");
		}

		auto& primitive = mesh.primitives[0];
		if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
		{
			throw std::runtime_error("ResourceLoader::load_model: only triangles are supported");
		}

		if (primitive.indices < 0)
		{
			throw std::runtime_error("ResourceLoader::load_model: no indices found");
		}

		const auto& index_accessor = model.accessors[primitive.indices];
		const auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];
		const auto& index_buffer = model.buffers[index_buffer_view.buffer];

		std::vector<uint32_t> indices = load_indices(index_accessor, index_buffer_view, index_buffer);

		const bool has_texture = [&primitive](){ return primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end(); }();

		Renderable renderable;
		MeshPtr new_mesh;
		if (has_bones)
		{
			new_mesh = std::make_unique<SkinnedMesh>(load_vertices<SkinnedVertices>(model, primitive), std::move(indices));
			if (!model.animations.empty())
			{
				retval.animations = load_animations(model, bones);
			}
			renderable.skeleton_id = ECS::get().add_skeleton(bones);
			renderable.pipeline_render_type = ERenderType::SKINNED;
		} else if (has_texture)
		{
			new_mesh = std::make_unique<TexMesh>(load_vertices<TexVertices>(model, primitive), std::move(indices));
			renderable.pipeline_render_type = ERenderType::STANDARD;
		} else 
		{
			new_mesh = std::make_unique<ColorMesh>(load_vertices<ColorVertices>(model, primitive), std::move(indices));
			renderable.pipeline_render_type = ERenderType::COLOR;
		}

		const auto mesh_id = MeshSystem::add(std::move(new_mesh));
		const auto mat_id = global_resource_loader.load_material(primitive, model);

		renderable.mesh_id = mesh_id;
		renderable.material_ids = { mat_id };
		retval.renderables.push_back(renderable);
	}

	return retval;
}

MaterialID ResourceLoader::load_texture(const std::string_view filename) 
{
	if (!std::filesystem::exists(filename))
	{
		throw std::runtime_error(fmt::format("ResourceLoader::load_texture: filename does not exist! {}", filename));
	}

	TextureMaterial material;
	material.data = std::make_unique<RawTextureDataSTB>(stbi_load(
		filename.data(), 
		(int*)(&material.width), 
		(int*)(&material.height), 
		(int*)(&material.channels), 
		STBI_rgb_alpha));
	
	// Since we are using STBI_rgb_alpha, to keep things simple we assume everything uses RGBA
	// Even if the actual image doesn't have an alpha channel
	assert(material.channels == 4 || material.channels == 3);
	material.channels = 4;

	if (!material.data.get())
	{
		throw std::runtime_error(fmt::format("failed to load texture image! {}", filename));
	}

	return MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(material)));
}