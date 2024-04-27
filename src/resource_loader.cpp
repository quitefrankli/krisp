#include "resource_loader.hpp"
#include "maths.hpp"
#include "objects/object.hpp"
#include "analytics.hpp"
#include "entity_component_system/skeletal.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/mesh.hpp"

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
ResourceLoader::TextureID ResourceLoader::global_texture_id_counter = 0;

struct RawTextureData
{
	virtual ~RawTextureData()
	{
	}

	virtual std::byte* get() = 0;
};

struct RawTextureDataSTB : public RawTextureData
{
	RawTextureDataSTB(stbi_uc* data)
	{
		this->data = reinterpret_cast<std::byte*>(data);
	}

	~RawTextureDataSTB() 
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

struct RawTextureDataGLFT : public RawTextureData
{
	RawTextureDataGLFT(std::vector<unsigned char>&& data) :
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

ResourceLoader::TextureData::~TextureData() 
{
}

ResourceLoader::~ResourceLoader() 
{
}

MaterialTexture ResourceLoader::fetch_texture(const std::string_view file)
{
	if (texture_name_to_id.find(file.data()) == texture_name_to_id.end())
	{
		load_texture(file);
	}

	const auto texture_id = texture_name_to_id[file.data()];
	TextureData& texture_data = cached_textures[texture_id];

	return create_material_texture(texture_data);
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
	ColorVertices vertices;
	vertices.reserve(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		SDS::ColorVertex vertex;
		vertex.pos = glm::vec3(pos_data[3 * i], pos_data[3 * i + 1], pos_data[3 * i + 2]);
		vertex.normal = glm::vec3(norm_data[3 * i], norm_data[3 * i + 1], norm_data[3 * i + 2]);
		vertices.push_back(vertex);
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
	TexVertices vertices;
	vertices.reserve(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		SDS::TexVertex vertex;
		vertex.pos = glm::vec3(pos_data[3 * i], pos_data[3 * i + 1], pos_data[3 * i + 2]);
		vertex.normal = glm::vec3(norm_data[3 * i], norm_data[3 * i + 1], norm_data[3 * i + 2]);
		vertex.texCoord = glm::vec2(tex_data[2 * i], tex_data[2 * i + 1]);
		vertices.push_back(vertex);
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

	if (joint_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || joint_accessor.type != TINYGLTF_TYPE_VEC4)
	{
		throw std::runtime_error("ResourceLoader: only unsigned byte vec4 joints are supported");
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

	const auto* pos_data = reinterpret_cast<const float*>(&pos_buffer.data[pos_accessor.byteOffset + pos_buffer_view.byteOffset]);
	const auto* norm_data = reinterpret_cast<const float*>(&norm_buffer.data[norm_accessor.byteOffset + norm_buffer_view.byteOffset]);
	const auto* joint_data = reinterpret_cast<const uint8_t*>(&joint_buffer.data[joint_accessor.byteOffset + joint_buffer_view.byteOffset]);
	const auto* weight_data = reinterpret_cast<const float*>(&weight_buffer.data[weight_accessor.byteOffset + weight_buffer_view.byteOffset]);
	const auto* tex_data = reinterpret_cast<const float*>(&tex_buffer.data[tex_accessor.byteOffset + tex_buffer_view.byteOffset]);

	SkinnedVertices vertices;
	vertices.reserve(pos_accessor.count);
	for (size_t i = 0; i < pos_accessor.count; ++i)
	{
		SDS::SkinnedVertex vertex;
		vertex.pos = glm::vec3(pos_data[3 * i], pos_data[3 * i + 1], pos_data[3 * i + 2]);
		vertex.normal = glm::vec3(norm_data[3 * i], norm_data[3 * i + 1], norm_data[3 * i + 2]);
		vertex.texCoord = glm::vec2(tex_data[2 * i], tex_data[2 * i + 1]);
		vertex.bone_ids = glm::vec4(joint_data[4 * i], joint_data[4 * i + 1], joint_data[4 * i + 2], joint_data[4 * i + 3]);
		vertex.bone_weights = glm::vec4(weight_data[4 * i], weight_data[4 * i + 1], weight_data[4 * i + 2], weight_data[4 * i + 3]);
		vertices.push_back(vertex);
	}

	return vertices;
}

MaterialID ResourceLoader::load_material(const tinygltf::Primitive& primitive, tinygltf::Model& model)
{
	Material new_material;
	if (primitive.material >= 0) // if it contains a material
	{
		const auto& mat = model.materials[primitive.material];
		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		// has color texture
		if (color_texture.index >= 0)
		{
			tinygltf::Image& image = model.images[color_texture.index];
			TextureData new_texture_data;
			new_texture_data.width = image.width;
			new_texture_data.height = image.height;
			new_texture_data.channels = image.component;
			new_texture_data.data = std::make_unique<RawTextureDataGLFT>(std::move(image.image));
			new_texture_data.texture_id = get_next_texture_id();

			auto pair = cached_textures.emplace(new_texture_data.texture_id, std::move(new_texture_data));
			new_material.texture = create_material_texture(pair.first->second);
		}

		new_material.material_data.diffuse = glm::vec3(
			mat.pbrMetallicRoughness.baseColorFactor[0],
			mat.pbrMetallicRoughness.baseColorFactor[1],
			mat.pbrMetallicRoughness.baseColorFactor[2]);
		new_material.material_data.ambient = new_material.material_data.diffuse;
		new_material.material_data.specular = (new_material.material_data.specular + new_material.material_data.diffuse)/2.0f;
		new_material.material_data.shininess = 1 - mat.pbrMetallicRoughness.roughnessFactor;
	}

	return MaterialSystem::add(std::make_unique<Material>(std::move(new_material)));
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

		if (node.children.size() > 4)
		{
			throw std::runtime_error("ResourceLoader: only 4 children per bone are supported");
		}

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

static std::unordered_map<std::string, std::vector<BoneAnimation>> load_animations(const tinygltf::Model& model, const std::vector<Bone>& initial_bones)
{
	if (model.skins.size() != 1)
	{
		throw std::runtime_error("ResourceLoader::load_animations: only one skin is supported");
	}
	
	std::unordered_map<std::string, std::vector<BoneAnimation>> final_animations;

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

		final_animations[animation.name] = std::move(new_bone_animations);
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

	LoadedModel retval;

	const bool has_bones = [&model](){ return !model.skins.empty(); }();
	if (has_bones)
	{
		retval.bones = load_bones(model);
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

		const bool has_texture = [&primitive]() { return primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end(); }();

		MeshPtr new_mesh;
		if (has_bones)
		{
			new_mesh = std::make_unique<SkinnedMesh>(load_vertices<SkinnedVertices>(model, primitive), std::move(indices));
			if (!model.animations.empty())
			{
				retval.animations = load_animations(model, retval.bones);
			}
		} else if (has_texture)
		{
			new_mesh = std::make_unique<TexMesh>(load_vertices<TexVertices>(model, primitive), std::move(indices));
		} else 
		{
			new_mesh = std::make_unique<ColorMesh>(load_vertices<ColorVertices>(model, primitive), std::move(indices));
		}

		const auto mesh_id = MeshSystem::add(std::move(new_mesh));
		const auto mat_id = load_material(primitive, model);

		Renderable renderable;
		renderable.mesh_id = mesh_id;
		renderable.material_ids = { mat_id };
		retval.renderables.push_back(renderable);
	}

	return retval;
}

void ResourceLoader::load_texture(const std::string_view file) 
{
	if (!std::filesystem::exists(file))
	{
		throw std::runtime_error(fmt::format("ResourceLoader::load_texture: file does not exist! {}", file));
	}

	TextureData new_texture;
	new_texture.data = std::make_unique<RawTextureDataSTB>(stbi_load(
		file.data(), 
		&new_texture.width, 
		&new_texture.height, 
		&new_texture.channels, 
		STBI_rgb_alpha));
	
	// Since we are using STBI_rgb_alpha, to keep things simple we assume everything uses RGBA
	// Even if the actual image doesn't have an alpha channel
	assert(new_texture.channels == 4 || new_texture.channels == 3);
	new_texture.channels = 4;

	if (!new_texture.data.get())
	{
		throw std::runtime_error(fmt::format("failed to load texture image! {}", file));
	}

	const auto new_texture_id = get_next_texture_id();
	new_texture.texture_id = new_texture_id;
	texture_name_to_id.emplace(file.data(), new_texture_id);
	cached_textures.emplace(new_texture_id, std::move(new_texture));
}

MaterialTexture ResourceLoader::create_material_texture(TextureData& texture_data)
{
	MaterialTexture texture;
	texture.data = texture_data.data->get();
	texture.width = texture_data.width;
	texture.height = texture_data.height;
	texture.channels = texture_data.channels;
	texture.texture_id = texture_data.texture_id;

	return texture;
}
