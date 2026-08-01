#pragma once

#include <entity_component_system/ecs.hpp>
#include <serialization/scene_resources.hpp>

#include <filesystem>
#include <ranges>

inline const std::filesystem::path &serialization_test_resource_directory()
{
	struct ScratchDirectory
	{
		ScratchDirectory() : path(std::filesystem::temp_directory_path() / "krisp_serialization_tests")
		{
			std::filesystem::create_directories(path);
		}
		~ScratchDirectory() { std::filesystem::remove_all(path); }
		std::filesystem::path path;
	};
	static const ScratchDirectory directory;
	return directory.path;
}

inline void serialize_ecs(ECS &ecs, Serializer &out)
{
	SceneResourceWriter resources(out, ecs, serialization_test_resource_directory());
	ecs.serialize(out, resources);
}

inline void deserialize_ecs(ECS &ecs, const Deserializer &in)
{
	SceneResourceReader resources(ecs, serialization_test_resource_directory());
	resources.prepare(in);
	ecs.deserialize(in, resources);
}

inline void serialize_renderables(ECS &ecs, Serializer &out)
{
	SceneResourceWriter resources(out, ecs, serialization_test_resource_directory());
	ecs.RenderableSystem::serialize(out, resources);
}

inline void deserialize_renderables(ECS &ecs, const Deserializer &in)
{
	SceneResourceReader resources(ecs, serialization_test_resource_directory());
	const auto keys = in.keys();
	if (std::ranges::find(keys, "resources") != keys.end())
		resources.prepare(in);
	ecs.RenderableSystem::deserialize(in, resources);
}

inline void serialize_colliders(ECS &ecs, Serializer &out)
{
	SceneResourceWriter resources(out, ecs, serialization_test_resource_directory());
	ecs.ColliderSystem::serialize(out, resources);
}

inline void deserialize_colliders(ECS &ecs, const Deserializer &in)
{
	SceneResourceReader resources(ecs, serialization_test_resource_directory());
	const auto keys = in.keys();
	if (std::ranges::find(keys, "resources") != keys.end())
		resources.prepare(in);
	ecs.ColliderSystem::deserialize(in, resources);
}
