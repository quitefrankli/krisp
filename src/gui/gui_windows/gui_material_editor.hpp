#pragma once

#include "gui_windows.hpp"

#include <glm/vec4.hpp>


class GuiMaterialEditor : public EngineUiWindow
{
public:
	GuiMaterialEditor();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	struct MaterialChange
	{
		RenderableID renderable_id;
		glm::vec4 base_color_factor{ 1.0f };
		float metallic_factor = 1.0f;
		float roughness_factor = 1.0f;
	};

	std::vector<std::string> renderable_labels;
	std::vector<RenderableID> renderable_ids;
	GuiVar<int> selected_renderable = 0;
	std::optional<ObjectID> target_object;
	std::optional<MaterialChange> pending_change;
	std::optional<std::string> load_error;
	std::string target_status = "Select an object";
	glm::vec4 base_color_factor{ 1.0f };
	float metallic_factor = 1.0f;
	float roughness_factor = 1.0f;
	bool compatible = false;
};
