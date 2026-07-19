#pragma once

#include "identifications.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

// Identity of data imported from a glTF/GLB document.  Runtime IDs are not
// stable across a scene reload; this record is the stable identity used by
// the scene serializer.
struct ImportedResourceProvenance
{
	std::string source;
	int scene = -1;
	int node = -1;
	int primitive = -1;
	int material = -1;
	int texture = -1;
	int skin = -1;
	int animation = -1;
};

class ResourceProvenance
{
public:
	static void register_mesh(MeshID id, ImportedResourceProvenance provenance);
	static void register_material(MaterialID id, ImportedResourceProvenance provenance);
	static void register_skeleton(SkeletonID id, ImportedResourceProvenance provenance);
	static void register_animation(AnimationID id, ImportedResourceProvenance provenance);

	static const ImportedResourceProvenance* mesh(MeshID id);
	static const ImportedResourceProvenance* material(MaterialID id);
	static const ImportedResourceProvenance* skeleton(SkeletonID id);
	static const ImportedResourceProvenance* animation(AnimationID id);
	static std::optional<SkeletonID> find_skeleton(const ImportedResourceProvenance& provenance);
	static std::optional<AnimationID> find_animation(const ImportedResourceProvenance& provenance);

	static void erase_mesh(MeshID id);
	static void erase_material(MaterialID id);
	static void clear();
};
