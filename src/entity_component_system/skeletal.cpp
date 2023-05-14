#include "skeletal.hpp"


std::vector<SDS::Bone> SkeletalComponent::get_bones_data() const
{
	std::vector<SDS::Bone> final_bones_data(bones.size());
	final_bones_data[0].final_transform = bones[0].relative_transform.get_mat4();
	for (uint32_t i = 1; i < bones.size(); ++i)
	{
		const auto& bone = bones[i];
		final_bones_data[i].final_transform = final_bones_data[bone.parent_node].final_transform * bone.relative_transform.get_mat4();
	}

	return final_bones_data;
}
