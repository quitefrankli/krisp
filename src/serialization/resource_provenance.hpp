#pragma once

#include "identifications.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

// Identity of data loaded from an external model or texture. Runtime IDs are
// not stable across a scene reload; this record is the stable identity used by
// the scene serializer.
enum class EExternalResourceKind
{
	Model,
	Texture,
};

struct ImportedResourceProvenance
{
	EExternalResourceKind kind = EExternalResourceKind::Model;
	std::string source;
	int scene = -1;
	int node = -1;
	int primitive = -1;
	int material = -1;
	int image = -1;
	int texture_semantic = -1;
	int skin = -1;
	int animation = -1;
};

struct PbrTextureOverride
{
	enum class Mode
	{
		Cleared,
		Replaced,
	};

	Mode mode = Mode::Cleared;
	MaterialID texture{0};
	PbrMaterial::TextureSampler sampler = PbrMaterial::TextureSampler::REPEAT;
};

struct ImportedPbrMaterialOverride
{
	struct SourceMaterial
	{
		glm::vec4 base_color_factor{1.0f};
		float metallic_factor = 1.0f;
		float roughness_factor = 1.0f;
		float normal_scale = 1.0f;
		PbrMaterial::TextureSlots textures;
	};

	ImportedResourceProvenance source;
	SourceMaterial original;
	std::optional<glm::vec4> base_color_factor;
	std::optional<float> metallic_factor;
	std::optional<float> roughness_factor;
	std::optional<float> normal_scale;
	std::optional<PbrTextureOverride> base_color_texture;
	std::optional<PbrTextureOverride> metallic_roughness_texture;
	std::optional<PbrTextureOverride> normal_texture;
};

class ResourceProvenance
{
public:
	static void register_mesh(MeshID id, ImportedResourceProvenance provenance);
	static void register_material(MaterialID id, ImportedResourceProvenance provenance);
	static void register_material_override(MaterialID id, ImportedPbrMaterialOverride material_override);
	static void register_skeleton(SkeletonID id, ImportedResourceProvenance provenance);
	static void register_animation(AnimationID id, ImportedResourceProvenance provenance);

	static const ImportedResourceProvenance* mesh(MeshID id);
	static const ImportedResourceProvenance* material(MaterialID id);
	static const ImportedPbrMaterialOverride* material_override(MaterialID id);
	static const ImportedResourceProvenance* skeleton(SkeletonID id);
	static const ImportedResourceProvenance* animation(AnimationID id);
	static std::optional<MeshID> find_mesh(
		const MeshSystem& meshes, const ImportedResourceProvenance& provenance);
	static std::optional<MaterialID> find_material(
		const MaterialSystem& materials, const ImportedResourceProvenance& provenance);
	static std::optional<SkeletonID> find_skeleton(const ImportedResourceProvenance& provenance);
	static std::optional<AnimationID> find_animation(const ImportedResourceProvenance& provenance);

	static void erase_mesh(MeshID id);
	static void erase_material(MaterialID id);
	static void erase_skeleton(SkeletonID id);
	static void erase_animation(AnimationID id);
	static void clear();
};
