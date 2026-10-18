#include "point_lighting.hpp"

#include "entity_component_system/light_source.hpp"
#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>


PointLightingData prepare_point_lighting(const std::span<const RenderLightState> lights)
{
	if (lights.size() > SDS::MAX_POINT_LIGHTS)
		throw std::invalid_argument("Render frame exceeds the eight point-light limit");

	PointLightingData result;
	result.light_count = static_cast<uint32_t>(lights.size());
	const std::array directions{
		Maths::right_vec, -Maths::right_vec,
		Maths::up_vec, -Maths::up_vec,
		Maths::forward_vec, -Maths::forward_vec,
	};
	const std::array ups{
		Maths::up_vec, Maths::up_vec,
		-Maths::forward_vec, Maths::forward_vec,
		Maths::up_vec, Maths::up_vec,
	};
	const auto projection = glm::perspectiveLH(
		Maths::deg2rad(90.0f), 1.0f, 0.1f, POINT_SHADOW_FAR_PLANE);
	for (size_t index = 0; index < lights.size(); ++index)
	{
		const auto& light = lights[index];
		validate_light_component(LightComponent{
			.intensity = light.intensity, .color = light.color,
		});
		if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y)
			|| !std::isfinite(light.position.z))
			throw std::invalid_argument("Point-light position must be finite");
		auto& data = result.lights[index];
		data.position = light.position;
		data.color = light.color;
		data.intensity = light.enabled ? light.intensity : 0.0f;
		data.shadow_slot = -1;
		if (!light.casts_shadow || data.intensity == 0.0f
			|| light.color == glm::vec3(0.0f))
			continue;
		data.shadow_slot = static_cast<int>(result.shadow_count++);
		for (size_t face = 0; face < directions.size(); ++face)
			data.shadow_view_proj_mats[face] = projection * glm::lookAtLH(
				light.position, light.position + directions[face], ups[face]);
	}
	return result;
}
