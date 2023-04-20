#pragma once

#include "game_engine.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "objects/cubemap.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility.hpp"
#include "analytics.hpp"
#include "interface/gizmo.hpp"
#include "hot_reload.hpp"
#include "gui/gui_manager.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"
#include "objects/light_source.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/Quill.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>


static DummyApplication dummy_app;

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>* GameEngine<GraphicsEngineTemplate>::global_engine = nullptr;

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>::GameEngine(std::function<void()>&& restart_signaller, App::Window& window) :
	restart_signaller(std::move(restart_signaller)),
	window(window),
	mouse(std::make_unique<Mouse<GameEngine>>(window)),
	gizmo(std::make_unique<Gizmo<GameEngine>>(*this)),
	graphics_engine(std::make_unique<GraphicsEngineT>(*this)),
	camera(std::make_unique<Camera>(Listener(), 
		   static_cast<float>(window.get_width())/static_cast<float>(window.get_height())))
{
	window.setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 0.0f, -2.0f));
	draw_object(camera->focus_obj);
	draw_object(camera->upvector_obj);
	experimental = std::make_unique<Experimental<GameEngine>>(*this);
	spawn_object<CubeMap>(); // background/horizon

	light_source = &spawn_object<LightSource>(glm::vec3(1.0f));
	light_source->set_position(glm::vec3(0.0f, 5.0f, 0.0f));
	add_clickable(light_source->get_id(), light_source);

	get_gui_manager().template spawn_gui<GuiMusic<GameEngine>>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(1e6 / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngineT::run, graphics_engine.get());
	Utility::sleep(std::chrono::milliseconds(100));

	if (!application)
	{
		application = &dummy_app;
	}

	application->on_begin();
	gizmo->init();

	Analytics analytics;
	analytics.text = "GameEngine: average cycle ms";

	std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();

	TPS_counter->start();
	Utility::LoopSleeper loop_sleeper(std::chrono::milliseconds(17));
	while (!should_shutdown && !window.should_close())
	{
#ifndef DISABLE_SLEEP
		loop_sleeper();
#endif
		const std::chrono::time_point<std::chrono::system_clock> new_time = std::chrono::system_clock::now();
		std::chrono::duration<float, std::milli> chrono_time_delta = new_time - time;
		const float time_delta = chrono_time_delta.count() * 0.001; // in seconds
		time = new_time;
		analytics.start();

		// for ticks per second
		TPS_counter->stop();
		TPS_counter->start();
		main_loop(time_delta);

		analytics.stop();
	}

	shutdown();
	graphics_engine_thread.join();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::main_loop(const float time_delta)
{
	window.poll_events();

	// poll gui stuff, we should take advantage of polymorphism later on, but for now this is relatively simple
	get_gui_manager().process(*this);

	if (mouse->rmb_down)
	{
		const float min_threshold = 0.001f;
		mouse->update_pos();
		glm::vec2 offset = mouse->get_prev_offset();
		float magnitude = glm::length(offset);
		if (magnitude > min_threshold)
		{
			camera->rotate_camera(offset, time_delta);
		}
	} else if (mouse->lmb_down)
	{
		const float sensitivity = 2.0f;
		const float min_threshold = 0.01f;

		// check w/ IMMEDIATE prev pos if offset is big enough
		if (mouse->update_pos_on_significant_offset(min_threshold))
		{
			const auto offset = mouse->get_orig_offset();
			glm::vec2 screen_axis(offset.x, offset.y);
			float magnitude = glm::length(screen_axis);

			const Maths::Ray r1 = camera->get_ray(mouse->orig_pos);
			const Maths::Ray r2 = camera->get_ray(mouse->curr_pos);
			gizmo->process(r1, r2);
		}
	} else if (mouse->mmb_down)
	{
		const float min_threshold = 0.01f;
		mouse->update_pos();
		const glm::vec2 offset_vec = mouse->get_orig_offset();
		const float magnitude = glm::length(offset_vec);
		if (magnitude > min_threshold)
		{
			camera->pan(offset_vec, magnitude);
		}
	}

	animator.process(time_delta);
	experimental->process(time_delta);
	application->on_tick(time_delta);
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->enqueue_cmd(std::make_unique<ShutdownCmd>());
}

template<template<typename> typename GraphicsEngineTemplate>
GameEngine<GraphicsEngineTemplate>::~GameEngine() = default;

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::delete_object(uint64_t id)
{
	objects.erase(id);
	graphics_engine->enqueue_cmd(std::make_unique<DeleteObjectCmd>(id));
	erase_from_menagerie(id);
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::highlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<StencilObjectCmd>(object));
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::unhighlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<UnStencilObjectCmd>(object));
}

template<template<typename> typename GraphicsEngineTemplate>
GuiManager<GameEngine<GraphicsEngineTemplate>>& GameEngine<GraphicsEngineTemplate>::get_gui_manager()
{
	return graphics_engine->get_gui_manager();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::restart()
{
	restart_signaller();
	shutdown();
}

template<template<typename> typename GraphicsEngineTemplate>
void GameEngine<GraphicsEngineTemplate>::send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	graphics_engine->enqueue_cmd(std::move(cmd));
}

template<template<typename> typename GraphicsEngineTemplate>
uint32_t GameEngine<GraphicsEngineTemplate>::get_window_width()
{
	return window.get_width();
}

template<template<typename> typename GraphicsEngineTemplate>
uint32_t GameEngine<GraphicsEngineTemplate>::get_window_height()
{
	return window.get_height();
}