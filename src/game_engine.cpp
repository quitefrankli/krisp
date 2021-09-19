#include "game_engine.hpp"

#include "objects.hpp"
#include "shapes.hpp"

#include <GLFW/glfw3.h>

#include <vector>
#include <thread>
#include <chrono>


GameEngine::GameEngine() :
	window(this),
	mouse(window),
	graphics_engine(*this),
	camera(graphics_engine.get_window_width<float>() / graphics_engine.get_window_width<float>())
{
	graphics_engine.binding_description = Vertex::get_binding_description();
	graphics_engine.attribute_descriptions = Vertex::get_attribute_descriptions();

	// shapes.emplace_back(Triangle());
	// shapes.emplace_back(Plane());

	// for (auto& shape : shapes)
	// {
	// 	graphics_engine.add_vertex_set(shape.get_vertices());
	// }

	std::vector<Object> objects;
	objects.emplace_back(Cube());

	for (auto& object : objects)
	{
		graphics_engine.insert_object(&object);
	}

	graphics_engine.setup();
}

void GameEngine::run()
{
	std::thread graphics_engine_thread(&GraphicsEngine::run, &graphics_engine);

	while (!should_shutdown && !glfwWindowShouldClose(get_window()))
	{
		glfwPollEvents();
		if (mouse.rmb_down) 
		{
			mouse.update_pos();
			float deg = (mouse.click_drag_orig_pos.x - mouse.current_pos.x) * 150;
			// std::cout << deg << '\n';
			get_camera().rotate_from_original_position(glm::vec3(0, 1, 0), deg);
			std::cout << get_camera().get_position().y << ' ' << get_camera().get_position().z << '\n';
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	shutdown();

	graphics_engine_thread.join();
}

void GameEngine::handle_window_callback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mode)
{
	GameEngine* engine = static_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window));
	engine->handle_window_callback_impl(glfw_window, key, scan_code, action, mode);
}

static int inc = 0;

void GameEngine::handle_window_callback_impl(GLFWwindow*, int key, int scan_code, int action, int mode)
{
	auto pressed_key = glfwGetKeyName(key, scan_code);

	printf("input detected [%d], key:=%d, scan_code:=%d, action:=%d, mode:=%d, translated_key:=%s\n", 
		   inc++, key, scan_code, action, mode, pressed_key ? pressed_key : "N/A");

	// if (glfwGetKey(window.get_window(), key) == GLFW_RELEASE)
	// {
	// 	return; // ignore key releases
	// }

	// if (action == GLFW_REPEAT)
	// {
	// 	return; // ignore held keys
	// }

	if (action == GLFW_RELEASE)
	{
		return; // ignore held keys
	}

	glm::vec3 pos;
	switch (key)
	{
		case GLFW_KEY_ESCAPE:
			shutdown();
			break;

		case GLFW_KEY_P:
			pos = get_camera().get_position();
			pos.z += 0.5f;
			get_camera().set_position(pos);
			break;

		case GLFW_KEY_L:
			pos = get_camera().get_position();
			pos.z -= 0.5f;
			get_camera().set_position(pos);
			break;		

		case GLFW_KEY_LEFT:
			get_camera().rotate_by(glm::vec3(1.0f, 0.0f, 0.0f), 5.0f);
			break;

		case GLFW_KEY_RIGHT:
			get_camera().rotate_by(glm::vec3(0.0f, 0.0f, 1.0f), -30.0f);
			break;

		default:
			break;
	}
}

void GameEngine::handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height) {
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_window_resize_callback_impl(glfw_window, width, height);
}

void GameEngine::handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height)
{
	graphics_engine.set_frame_buffer_resized();
}

void GameEngine::handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_mouse_button_callback_impl(glfw_window, button, action, mode);
}

void GameEngine::handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		mouse.rmb_down = true;
		mouse.update_pos();
		mouse.click_drag_orig_pos = mouse.current_pos;
		camera.set_original_position(camera.get_position());
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
	{
		mouse.rmb_down = false;
		get_camera().reset_position();
	}
}

void GameEngine::shutdown_impl()
{
	should_shutdown = true;
	graphics_engine.shutdown();
}