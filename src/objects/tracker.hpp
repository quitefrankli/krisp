#pragma once

#include "object.hpp"

#include <glm/gtx/transform.hpp>

#include <cassert>


class ITrackableObject
{
public:
	ITrackableObject(Object* obj) : object(obj)
	{
		assert(obj);
	}
	
	virtual void update_tracker()
	{
		position = object->get_position();
		scale = object->get_scale();
		rotation = object->get_rotation();
	}

	virtual glm::vec3 get_old_position() const { return position; }
	virtual glm::vec3 get_old_scale() const { return scale; }
	virtual glm::quat get_old_rotation() const { return rotation; }
	virtual glm::mat4 get_old_transform() const
	{
		glm::mat4 transform = glm::scale(glm::mat4_cast(glm::normalize(rotation)), scale);
		transform[3] = glm::vec4(position, 1.0f);
		return transform;
	}
	
	virtual void set_old_position(const glm::vec3& pos) { position = pos; }
	virtual void set_old_scale(const glm::vec3& scale) { this->scale = scale; }
	virtual void set_old_rotation(const glm::quat& rot) { rotation = rot; }
	
private:
	Object* object = nullptr;
	glm::vec3 position;
	glm::vec3 scale;
	glm::quat rotation;
};