#pragma once

#include "maths.hpp"


class GuiWindow
{
public:
	virtual void draw() = 0;
};

class GuiGraphicsSettings : public GuiWindow
{
public:
	GuiGraphicsSettings();

	void draw() override;

public:
	float light_strength = 1.0f;
	Maths::Ray light_ray;	
};