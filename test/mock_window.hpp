#include <window.hpp>
#include <iwindow_callbacks.hpp>


class MockWindow : public App::Window
{
public:
	MockWindow() :
		Window(false)
	{
	}

	virtual void setup_callbacks(IWindowCallbacks& callbacks) override
	{
	}

	virtual void poll_events() override
	{
	}

	virtual bool should_close() override
	{
		return false;
	}

	virtual glm::vec2 get_cursor_pos() override
	{
		return cursor_position;
	}

	void set_cursor_pos(glm::vec2 position)
	{
		cursor_position = position;
	}

private:
	glm::vec2 cursor_position{ 0.0f };
};
