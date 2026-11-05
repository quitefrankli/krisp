#include "renderer.hpp"
#include "renderers.hpp"
#include "graphics_engine/dynamic_rendering.hpp"


GuiRenderer::GuiRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
}

GuiRenderer::~GuiRenderer()
{
}

void GuiRenderer::allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view)
{
	presentation_images.push_back(presentation_image);
	presentation_image_views.push_back(presentation_image_view);
}

void GuiRenderer::submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index)
{
	const VkImage image = presentation_images.at(frame_index);
	const VkImageView image_view = presentation_image_views.at(frame_index);
	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	VkRenderingAttachmentInfo color_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	color_info.imageView = image_view;
	color_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
	rendering_info.renderArea = {{0, 0}, get_extent()};
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_info;
	vkCmdBeginRendering(command_buffer, &rendering_info);
	get_graphics_engine().get_graphics_gui_manager().add_render_cmd(command_buffer);
	vkCmdEndRendering(command_buffer);
	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
}
