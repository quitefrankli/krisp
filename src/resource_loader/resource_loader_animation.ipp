#include "identifications.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include <stdexcept>
#include <optional>
#include <vector>
#include <map>


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