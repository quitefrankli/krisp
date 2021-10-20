#include "game_engine.hpp"

#include "camera.hpp"
#include "objects.hpp"
#include "shapes.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"

#include <tiny_obj_loader.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


GameEngine::GameEngine() :
	window(this),
	mouse(window)
{
	graphics_engine = std::make_unique<GraphicsEngine>(*this);

	camera = std::make_unique<Camera>(graphics_engine->get_window_width<float>() / graphics_engine->get_window_height<float>());

	graphics_engine->setup();
}

void GameEngine::run()
{
	std::thread graphics_engine_thread(&GraphicsEngine::run, graphics_engine.get());
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	Analytics analytics;
	analytics.text = "GameEngine: average cycle ms";

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string path = "../resources/models/viking_room.obj";
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str()))
	{
		throw std::runtime_error(err);
	}

	Object mesh;
	for (auto& shape : shapes)
	{
		Shape new_shape;
		for (auto& index : shape.mesh.indices)
		{
			Vertex new_vertex;
			new_vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};
			new_vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};
			new_shape.vertices.push_back(std::move(new_vertex));
		}
		mesh.shapes.push_back(std::move(new_shape));
	}

	{
		// Cube cube;
		// mesh = cube;
		SpawnObjectCmd spawn_obj_cmd;
		spawn_obj_cmd.object_id = mesh.get_id();
		spawn_obj_cmd.object = mesh;
		graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(spawn_obj_cmd));
		objects.push_back(std::move(mesh));
	}

	while (!should_shutdown && !glfwWindowShouldClose(get_window()))
	{
		analytics.start();

		glfwPollEvents();
		if (mouse.rmb_down)
		{
			mouse.update_pos();
			float rot_x_axis = -(mouse.click_drag_orig_pos.y - mouse.current_pos.y);
			float rot_y_axis = -(mouse.click_drag_orig_pos.x - mouse.current_pos.x);
			glm::vec3 vec(rot_x_axis, rot_y_axis, 0.0f);
			float magnitude = glm::length(vec) * 2.0f;
			vec = glm::normalize(vec);

			glm::mat4 orig = camera->get_original_transformation();
			glm::mat4 curr = camera->get_transformation();
			glm::quat quaternion = glm::angleAxis(magnitude, vec);
			glm::mat4 transform = glm::mat4_cast(quaternion);
			glm::mat4 final_transform = transform * orig;
			// glm::mat4 final_transform = orig * transform; // order matters! 
			if (magnitude > 0) {
				camera->set_transformation(final_transform);
			}
		} else if (mouse.lmb_down)
		{
			mouse.update_pos();
			float rot_x_axis = -(mouse.click_drag_orig_pos.y - mouse.current_pos.y);
			float rot_y_axis = -(mouse.click_drag_orig_pos.x - mouse.current_pos.x);
			glm::vec3 vec(rot_x_axis, rot_y_axis, 0.0f);
			float magnitude = glm::length(vec) * 2.0f;
			vec = glm::normalize(vec);

			glm::mat4 orig = objects[0].get_original_transformation();
			glm::mat4 curr = objects[0].get_transformation();
			glm::quat quaternion = glm::angleAxis(magnitude, vec);
			glm::mat4 transform = glm::mat4_cast(quaternion);
			glm::mat4 final_transform = transform * orig;
			// glm::mat4 final_transform = orig * transform; // order matters! 
			if (magnitude > 0) {
				objects[0].set_transformation(final_transform);
			}

			UpdateObjectUniformsCmd cmd;
			cmd.object_id = objects[0].get_id();
			cmd.transformation = objects[0].get_transformation();
			graphics_engine->enqueue_cmd(std::make_unique<UpdateObjectUniformsCmd>(cmd));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		analytics.stop();
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
			break;

		case GLFW_KEY_RIGHT:
			break;

		case GLFW_KEY_N:
		{
			ChangeTextureCmd cmd;
			cmd.filename = "texture2.jpg";
			graphics_engine->enqueue_cmd(std::make_unique<ChangeTextureCmd>(cmd));
			break;
		}

		case GLFW_KEY_S:
		{
			{
				Cube cube;
				cube.set_transformation(glm::translate(cube.get_transformation(), glm::vec3(0.5f, 0.0f, 0.0f) * float(objects.size())));
				SpawnObjectCmd spawn_obj_cmd;
				spawn_obj_cmd.object_id = cube.get_id();
				spawn_obj_cmd.object = cube;
				graphics_engine->enqueue_cmd(std::make_unique<SpawnObjectCmd>(spawn_obj_cmd));
				objects.push_back(std::move(cube));
				break;
			}
		}

		default:
			break;
	}
}

void GameEngine::handle_window_resize_callback(GLFWwindow* glfw_window, int width, int height) {
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_window_resize_callback_impl(glfw_window, width, height);
}

void GameEngine::handle_window_resize_callback_impl(GLFWwindow* glfw_window, int width, int height)
{
	// graphics_engine->set_frame_buffer_resized();
	// TODO handle resizing of window
}

void GameEngine::handle_mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mode)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_mouse_button_callback_impl(glfw_window, button, action, mode);
}

void GameEngine::handle_mouse_button_callback_impl(GLFWwindow* glfw_window, int button, int action, int mode)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (action == GLFW_PRESS)
		{
			mouse.rmb_down = true;
			mouse.update_pos();
			mouse.click_drag_orig_pos = mouse.current_pos;
			get_camera().set_original_transformation(get_camera().get_transformation());
			// get_camera().set_original_position(get_camera().get_position());
		} else if (action == GLFW_RELEASE)
		{
			mouse.rmb_down = false;
			// get_camera().reset_position();
		}
	} else if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			mouse.lmb_down = true;
			mouse.update_pos();
			mouse.click_drag_orig_pos = mouse.current_pos;
		} else if (action == GLFW_RELEASE)
		{
			mouse.lmb_down = false;
			objects[0].set_original_transformation(objects[0].get_transformation());
		}
	}
}

void GameEngine::handle_scroll_callback(GLFWwindow* glfw_window, double xoffset, double yoffset)
{
	reinterpret_cast<GameEngine*>(glfwGetWindowUserPointer(glfw_window))->handle_scroll_callback_impl(glfw_window, xoffset, yoffset);
}

void GameEngine::handle_scroll_callback_impl(GLFWwindow* glfw_window, double xoffset, double yoffset)
{
	float sensitivity = -0.2f;
	glm::vec3 new_pos = get_camera().get_position() + glm::vec3(0.0f, 0.0f, yoffset) * sensitivity;
	get_camera().set_position(new_pos);
}

void GameEngine::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->enqueue_cmd(std::make_unique<ShutdownCmd>());
}

GameEngine::~GameEngine() = default;