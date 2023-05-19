#pragma once

#include "collider.hpp"

#include <glm/vec3.hpp>

#include <unordered_map>
#include <functional>


struct CollisionResult
{
	bool bCollided = false;
	glm::vec3 intersection;
};

struct CollisionType
{
	ECollider collider1;
	ECollider collider2;

	bool operator==(const CollisionType& other) const
	{
		return collider1 == other.collider1 && collider2 == other.collider2;
	}
};

template<>
struct std::hash<CollisionType>
{
	std::size_t operator()(const CollisionType& type) const
	{
		return std::hash<int>()(static_cast<int>(type.collider1)) ^ 
			std::hash<int>()(static_cast<int>(type.collider2));
	}
};

class CollisionDetector
{
public:
	using SpecialisedCollisionDetector = std::function<CollisionResult(const Collider*, const Collider*)>;

	static CollisionResult check_collision(const Collider* collider1, const Collider* collider2);

	static void add_collision_detector(const CollisionType& collision_type, SpecialisedCollisionDetector detector)
	{
		detectors.emplace(collision_type, std::move(detector));
	}

	static void remove_collision_detector(const CollisionType& collision_type)
	{
		detectors.erase(collision_type);
	}

private:
	CollisionDetector();

	static CollisionDetector instance;

	static std::unordered_map<CollisionType, SpecialisedCollisionDetector> detectors;
};