#pragma once

#include <string>
#include <memory>


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
	std::string filename;

	void process(GraphicsEngine* engine) override;
};

class ToggleFPSCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};