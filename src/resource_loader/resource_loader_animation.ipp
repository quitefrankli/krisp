#include "resource_loader.hpp"
#include "identifications.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include <stdexcept>
#include <vector>

struct SamplerKey
{
	glm::vec4 value{0.0f};
	glm::vec4 in_tangent{0.0f};
	glm::vec4 out_tangent{0.0f};
	float time = 0.0f;
};

struct SamplerData
{
	BoneAnimation::Interpolation interpolation = BoneAnimation::Interpolation::LINEAR;
	std::vector<SamplerKey> keys;
};

struct ImportedAnimation
{
	std::string name;
	std::vector<BoneAnimation> bone_animations;
	int source_index = -1;
};

SamplerData get_sampler_data(
	const tinygltf::Model& model, 
	const tinygltf::AnimationSampler& sampler)
{
	SamplerData retval;
	if (sampler.interpolation.empty() || sampler.interpolation == "LINEAR")
		retval.interpolation = BoneAnimation::Interpolation::LINEAR;
	else if (sampler.interpolation == "STEP")
		retval.interpolation = BoneAnimation::Interpolation::STEP;
	else if (sampler.interpolation == "CUBICSPLINE")
		retval.interpolation = BoneAnimation::Interpolation::CUBIC_SPLINE;
	else
		throw ResourceLoadError("ResourceLoader: unsupported animation interpolation '" + sampler.interpolation + "'");

	if (sampler.input < 0 || sampler.input >= model.accessors.size())
	{
		throw ResourceLoadError("ResourceLoader: invalid input accessor");
	}

	if (sampler.output < 0 || sampler.output >= model.accessors.size())
		throw ResourceLoadError("ResourceLoader: invalid output accessor");
	const auto& input_accesor = model.accessors[sampler.input];
	const auto& output_accessor = model.accessors[sampler.output];
	
	if (input_accesor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || input_accesor.type != TINYGLTF_TYPE_SCALAR)
	{
		throw ResourceLoadError("ResourceLoader: only float scalar input accessors are supported");
	}

	if (output_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || 
		(output_accessor.type != TINYGLTF_TYPE_VEC3 && output_accessor.type != TINYGLTF_TYPE_VEC4))
	{
		throw ResourceLoadError("ResourceLoader: only float vec3 and vec4 output accessors are supported");
	}

	GltfImport::AccessorReader input_reader(model, sampler.input);
	GltfImport::AccessorReader output_reader(model, sampler.output);
	const size_t outputs_per_key = retval.interpolation == BoneAnimation::Interpolation::CUBIC_SPLINE ? 3 : 1;
	if (output_accessor.count != input_accesor.count * outputs_per_key)
		throw ResourceLoadError("ResourceLoader: animation output count does not match its interpolation mode");

	retval.keys.reserve(input_accesor.count);
	const auto read_value = [&](const size_t index)
	{
		glm::vec4 value(0.0f);
		const size_t components = output_accessor.type == TINYGLTF_TYPE_VEC3 ? 3 : 4;
		for (size_t component = 0; component < components; ++component)
			value[component] = output_reader.number(index, component);
		return value;
	};
	for (size_t i = 0; i < input_accesor.count; ++i)
	{
		SamplerKey key;
		key.time = input_reader.number(i, 0);
		if (retval.interpolation == BoneAnimation::Interpolation::CUBIC_SPLINE)
		{
			key.in_tangent = read_value(i * 3);
			key.value = read_value(i * 3 + 1);
			key.out_tangent = read_value(i * 3 + 2);
		}
		else
		{
			key.value = read_value(i);
		}
		retval.keys.push_back(key);
	}

	return retval;
}

static std::vector<ImportedAnimation> import_animations(
	const tinygltf::Model& model,
	const std::vector<Bone>& initial_bones,
	const int skin_index,
	const std::vector<size_t>& source_joint_to_target)
{
	const auto& skin = model.skins.at(skin_index);
	if (source_joint_to_target.size() != skin.joints.size())
		throw ResourceLoadError("ResourceLoader: animation joint mapping size differs from source skin");
	std::vector<ImportedAnimation> final_animations;

	const std::map<int, size_t> node_to_joint = [&skin, &source_joint_to_target] {

		std::map<int, size_t> retval;
		for (size_t i = 0; i < skin.joints.size(); i++)
		{
			retval[skin.joints[i]] = source_joint_to_target[i];
		}
		return retval;
	}();

	for (size_t animation_index = 0; animation_index < model.animations.size(); ++animation_index)
	{	
		const auto& animation = model.animations[animation_index];
		// An animation represents a collection of BoneAnimations, each of which is a collection of keyframes
		std::vector<BoneAnimation> new_bone_animations(initial_bones.size());
		for (size_t bone_idx = 0; bone_idx < initial_bones.size(); ++bone_idx)
			new_bone_animations[bone_idx].base_transform = initial_bones[bone_idx].relative_transform;
		bool has_joint_channels = false;

		for (const auto& channel : animation.channels)
		{
			if (channel.target_node < 0 || channel.target_node >= model.nodes.size())
			{
				throw ResourceLoadError("ResourceLoader: invalid target node");
			}

			const auto node_it = node_to_joint.find(channel.target_node);
			if (node_it == node_to_joint.end())
			{
				// glTF animations may also target helper nodes outside this skin.
				continue;
			}
			if (channel.target_path != "rotation" && channel.target_path != "translation" && channel.target_path != "scale")
				throw ResourceLoadError("ResourceLoader: only rotation, translation and scale target paths are supported");
			has_joint_channels = true;
			const auto joint_idx = node_it->second;
			if (channel.sampler < 0 || channel.sampler >= animation.samplers.size())
				throw ResourceLoadError("ResourceLoader: invalid animation sampler");

			auto sampler_data = get_sampler_data(model, animation.samplers[channel.sampler]);
			auto& bone_animation = new_bone_animations[joint_idx];
			if (channel.target_path == "rotation")
			{
				for (auto& key : sampler_data.keys)
				{
					key.value = GltfImport::quaternion_to_krisp_basis(key.value);
					key.in_tangent = GltfImport::quaternion_to_krisp_basis(key.in_tangent);
					key.out_tangent = GltfImport::quaternion_to_krisp_basis(key.out_tangent);
				}
				bone_animation.rotation_track.interpolation = sampler_data.interpolation;
				for (const auto& key : sampler_data.keys)
					bone_animation.rotation_track.keys.push_back({ key.time, key.value, key.in_tangent, key.out_tangent });
			}
			else
			{
				if (channel.target_path == "translation")
				{
					for (auto& key : sampler_data.keys)
					{
						key.value.x = -key.value.x;
						key.in_tangent.x = -key.in_tangent.x;
						key.out_tangent.x = -key.out_tangent.x;
					}
				}
				auto& track = channel.target_path == "translation"
					? bone_animation.translation_track : bone_animation.scale_track;
				track.interpolation = sampler_data.interpolation;
				for (const auto& key : sampler_data.keys)
					track.keys.push_back({ key.time, glm::vec3(key.value), glm::vec3(key.in_tangent), glm::vec3(key.out_tangent) });
			}
			for (const auto& key : sampler_data.keys)
			{
				bone_animation.animation_start_secs =
					std::min(bone_animation.animation_start_secs, key.time);
				bone_animation.animation_end_secs =
					std::max(bone_animation.animation_end_secs, key.time);
			}
		}
		if (!has_joint_channels)
			continue;

		final_animations.push_back({
			animation.name.empty() ? "Animation " + std::to_string(animation_index + 1) : animation.name,
			std::move(new_bone_animations), static_cast<int>(animation_index)
		});
	}

	return final_animations;
}
