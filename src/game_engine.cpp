#include "game_engine.hpp"

#include <vector>
#include <thread>
#include <chrono>

#include <GLFW/glfw3.h>


GameEngine::GameEngine() :
	window(this),
	graphics_engine(*this),
	camera(graphics_engine.get_window_width<float>() / graphics_engine.get_window_width<float>())
{
	std::vector<Vertex> vertices
	{
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, 
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
	};
	graphics_engine.set_vertices(vertices);
	graphics_engine.binding_description = Vertex::get_binding_description();
	graphics_engine.attribute_descriptions = Vertex::get_attribute_descriptions();

	graphics_engine.setup();
}

void GameEngine::run()
{
	std::thread graphics_engine_thread(&GraphicsEngine::run, &graphics_engine);

	while (!should_shutdown && !glfwWindowShouldClose(get_window()))
	{
		glfwPollEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	shutdown();

	graphics_engine_thread.join();
}

void GameEngine::run_impl()
{
	
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
			get_camera().rotate_by(glm::vec3(1.0f, 0.0f, 0.0f), 30.0f);
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

void GameEngine::shutdown_impl()
{
	should_shutdown = true;
	graphics_engine.shutdown();
}