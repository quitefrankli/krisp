#pragma once

#include "gui_windows.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


class GuiMeshEditor : public EngineUiWindow
{
public:
	GuiMeshEditor();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	enum class Action
	{
		ADD,
		REMOVE,
		REPLACE,
		SET_VISIBILITY,
	};

	struct Request
	{
		Action action;
		ObjectID object_id;
		std::optional<RenderableID> target_id;
		std::optional<RenderableID> source_id;
		bool visible = true;
	};

	struct RenderableRow
	{
		RenderableID id;
		std::string name;
		bool visible = true;
	};

	void queue(Request request);
	void refresh(GameEngine& engine);

	std::optional<ObjectID> target_object;
	std::optional<Request> pending_request;
	std::vector<RenderableRow> rows;
	std::optional<std::string> error;
	std::string target_status = "Select an object with the gizmo";
	std::uint64_t source_id_value = 0;
	bool edit_requested = false;
	bool object_selected = false;
	bool source_id_valid = false;
};
