#pragma once

#include <glm/vec2.hpp>


namespace App 
{
	class Window;
}

enum class EMouseButton
{
	LEFT,
	RIGHT,
	MIDDLE,
};

enum class EKeyModifier
{
	NONE,
	CTRL,
	SHIFT,
	ALT,
	CTRL_SHIFT,
	CTRL_ALT,
	SHIFT_ALT,
	CTRL_SHIFT_ALT,
};

enum class EInputAction
{
	PRESS,
	RELEASE,
	REPEAT,
};

struct MouseInput
{
	EMouseButton button;
	EKeyModifier modifier;
	EInputAction action;

	bool eq(EMouseButton button, EKeyModifier modifier, EInputAction action) const
	{
		return this->button == button &&
			   this->modifier == modifier &&
			   this->action == action;
	}

	bool operator==(const MouseInput& other) const
	{
		return eq(other.button, other.modifier, other.action);
	}
};

class Mouse
{
public:
	Mouse(App::Window& window);

	glm::vec2 orig_pos; // click drag pos
	glm::vec2 prev_pos; // time delta pos
	glm::vec2 curr_pos;
	bool rmb_down = false;
	bool lmb_down = false;
	bool mmb_down = false;

	bool update_pos_on_significant_offset(const float min_offset);
	glm::vec2 update_pos();
	glm::vec2 get_prev_offset();
	glm::vec2 get_orig_offset();
	glm::vec2 get_curr_pos() const;
	
private:
	App::Window* window = nullptr;
};

struct KeyInput
{
	int key;
	EKeyModifier modifier;
	EInputAction action;

	bool eq(int key, EKeyModifier modifier, EInputAction action) const
	{
		return this->key == key &&
			   this->modifier == modifier &&
			   this->action == action;
	}

    bool operator==(const KeyInput& other) const
    {
        return eq(other.key, other.modifier, other.action);
    }
};
