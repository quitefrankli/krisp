#pragma once

#include "shapes.hpp"

#include <glm/gtc/quaternion.hpp>

#include <vector>


class GraphicsEngineObject;
class ResourceLoader;

class ObjectAbstract
{
public:
	ObjectAbstract(const ObjectAbstract& object) = delete;
	ObjectAbstract& operator=(const ObjectAbstract& object) = delete;

	ObjectAbstract();
	ObjectAbstract(uint64_t id);
	ObjectAbstract(ObjectAbstract&& object) noexcept = default;
	~ObjectAbstract() = default;

	uint64_t get_id() const { return id; }
	// void set_id(uint64_t id) { this->id = id; } ;// we don't really want to set id ever
	void generate_new_id() { id = global_id++; }

private:
	static uint64_t global_id;
	uint64_t id;
};

class Object : public ObjectAbstract
{
public:
	Object(const Object& object) = delete;

	Object() = default;
	Object(Object&& object) noexcept = default;
	Object(ResourceLoader& loader, std::string mesh, std::string texture);
	Object(std::string texture);

	std::vector<Shape> shapes;
	std::vector<std::vector<Vertex>>& get_vertex_sets();

	std::string texture;

	void toggle_visibility() { bVisible = !bVisible; }
	bool get_visibility() const { return bVisible; }
	void generate_normals();

	bool is_colliding();

public:
	virtual glm::mat4 get_transform();
	virtual glm::vec3 get_position() { return position; }
	virtual glm::vec3 get_scale() { return scale; }
	virtual glm::quat get_rotation() { return orientation; }

	virtual void set_transform(glm::mat4& transform);
	virtual void set_position(glm::vec3& position);
	virtual void set_scale(glm::vec3& scale);
	virtual void set_rotation(glm::quat& rotation);

private:
	bool is_transform_old = true;
	bool is_vertex_sets_old = true;
	std::vector<std::vector<Vertex>> cached_vertex_sets;
	glm::mat4 cached_transform;
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);
	glm::quat orientation; // default init creates identity quaternion
	bool bVisible = true;
};

class Pyramid : public Object
{
public:
	Pyramid();
	Pyramid(Pyramid&& pyramid) noexcept = default;
};

class Cube : public Object
{
public:
	Cube();
	Cube(std::string texture);
	Cube(Cube&& cube) noexcept = default;

private:
	void init();
};

// note that this is procedurally generated and very slow
class Sphere : public Object
{
public:
	Sphere();
	Sphere(Sphere&& cube) noexcept = default;
};

class HollowCylinder : public Object
{
public:
	HollowCylinder();
	HollowCylinder(HollowCylinder&& hollow_cylinder) noexcept = default;

	// for tower of hanoi
	int pillar_index = 0;
};

class Cylinder : public Object
{
public:
	Cylinder();
	Cylinder(Cylinder&& cylinder) noexcept = default;

	// for tower of hanoi
	float content_height = 0.0f;
};