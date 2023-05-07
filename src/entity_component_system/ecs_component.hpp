#pragma once

#include "objects/object_id.hpp"

#include <unordered_set>


class ECSManager;

class ECSComponent
{
public:
	ECSComponent(ECSManager& ecs_manager) : ecs_manager(ecs_manager) {}

	virtual ~ECSComponent() = default;

	virtual void process(const float delta_secs) {};

protected:
	ECSManager& ecs_manager;
};