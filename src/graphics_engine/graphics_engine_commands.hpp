#pragma once

#include "objects.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <memory>


class Object;
class GraphicsEngine;
class GraphicsEngineCommand;

using GraphicsEngineCommandPtr = std::unique_ptr<GraphicsEngineCommand>;

class GraphicsEngineCommand
{
public:
	virtual void process(GraphicsEngine* engine) = 0;
};

class ChangeTextureCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;

	std::string filename;
};

class ToggleFPSCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};

class ObjectCommand : public GraphicsEngineCommand
{
public:
	uint64_t object_id;
};

class UpdateObjectUniformsCmd : public ObjectCommand
{
public:
	void process(GraphicsEngine* engine) override;

	glm::mat4 transformation;
};

class SpawnObjectCmd : public ObjectCommand
{
public:
	void process(GraphicsEngine* engine) override;

	Object object;
};