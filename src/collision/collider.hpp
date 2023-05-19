#pragma once

#include "maths.hpp"


enum class ECollider
{
	// MAIN COLLIDERS
	RAY,
	SPHERE,
	// BOX,
	// CAPSULE,

	// CUSTOM COLLIDERS

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

struct SphereCollider : public Collider
{
	SphereCollider() = default;
	SphereCollider(const Maths::Sphere& sphere) : data(sphere) {}
	virtual ECollider get_type() const override { return ECollider::SPHERE; }
	Maths::Sphere get_data() const;

private:
	Maths::Sphere data;
};
