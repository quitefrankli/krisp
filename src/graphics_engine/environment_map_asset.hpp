#pragma once

#include "environment_map_processor.hpp"

#include <array>
#include <filesystem>


// Versioned, source-validated offline representation of the textures produced
// by EnvironmentMapProcessor.
class EnvironmentMapAsset
{
public:
	static ProcessedEnvironment read(const std::filesystem::path &path,
	                                 const std::array<EnvironmentFace, 6> &source_faces,
	                                 const EnvironmentMapProcessor::Settings &settings = {});

	static void write(const std::filesystem::path &path, const std::array<EnvironmentFace, 6> &source_faces,
	                  const ProcessedEnvironment &environment, const EnvironmentMapProcessor::Settings &settings = {});
};
