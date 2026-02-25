#pragma once

#include "maths.hpp"

#include <glm/vec2.hpp>


enum class ECollider
{
	RAY,
	SPHERE,
	// BOX,
	// CAPSULE,
	QUAD,
};

struct Collider
{
	virtual ECollider get_type() const = 0;
	virtual void apply_transform(const Maths::Transform& transform) {}

	void set_temporary_transform(const Maths::Transform& transform) const { temporary_transform = transform; }
	void clear_temporary_transform() const { temporary_transform = Maths::Transform{}; }
	
protected:
	const Maths::Transform& get_temporary_transform() const { return temporary_transform; }

private:
	// small optimisation to avoid creating a new transform every frame
	mutable Maths::Transform temporary_transform;
};

struct RayCollider : public Collider
{
	RayCollider(const Maths::Ray& ray) : data(ray) {}
	virtual ECollider get_type() const override { return ECollider::RAY; }
	Maths::Ray get_data() const;

private:
	Maths::Ray data;
};

struct QuadCollider : public Collider
{
	QuadCollider() = default;
	QuadCollider(const Maths::Plane& plane, const glm::vec2& size) : data(plane), size(size) {}
	virtual ECollider get_type() const override { return ECollider::QUAD; }

	Maths::Plane get_data() const;
	bool check_collision(const RayCollider& ray) const;
	bool check_collision(const RayCollider& ray, glm::vec3& out_intersection) const;

private:
	bool is_point_in_quad_bounds(const glm::vec3& point, const Maths::Plane& plane) const;

	Maths::Plane data;
	glm::vec2 size = glm::vec2(1.0f);
};

struct SphereCollider : public Collider
{
	SphereCollider() = default;
	SphereCollider(const Maths::Sphere& sphere) : data(sphere) {}
	virtual ECollider get_type() const override { return ECollider::SPHERE; }
	Maths::Sphere get_data() const;

private:
	Maths::Sphere data;
};
