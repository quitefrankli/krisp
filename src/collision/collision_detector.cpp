#include "collision_detector.hpp"


std::unordered_map<CollisionType, CollisionDetector::SpecialisedCollisionDetector> CollisionDetector::detectors;
CollisionDetector CollisionDetector::instance;

CollisionResult CollisionDetector::check_collision(const Collider* collider1, const Collider* collider2)
{
	const auto type1 = collider1->get_type();
	const auto type2 = collider2->get_type();
	auto it = detectors.find(CollisionType{ type1, type2 });
	if (it != detectors.end())
	{
		return it->second(collider1, collider2);
	}

	it = detectors.find(CollisionType{ type2, type1 });
	if (it != detectors.end())
	{
		return it->second(collider2, collider1);
	}

	return CollisionResult{};
}

CollisionDetector::CollisionDetector()
{
	CollisionType ray_sphere{ ECollider::RAY, ECollider::SPHERE };
	detectors.emplace(ray_sphere, [](const Collider* collider1, const Collider* collider2) -> CollisionResult
	{
		const auto* ray = static_cast<const RayCollider*>(collider1);
		const auto* sphere = static_cast<const SphereCollider*>(collider2);

		auto res = Maths::ray_sphere_collision(sphere->get_data(), ray->get_data());

		return {
			res.has_value(),
			res.value_or(glm::vec3(0.0f))
		};
	});

	CollisionType ray_quad{ ECollider::RAY, ECollider::QUAD };
	detectors.emplace(ray_quad, [](const Collider* collider1, const Collider* collider2) -> CollisionResult
	{
		const auto* ray = static_cast<const RayCollider*>(collider1);
		const auto* quad = static_cast<const QuadCollider*>(collider2);

		glm::vec3 intersection{};
		const bool collided = quad->check_collision(*ray, intersection);

		return {
			collided,
			intersection
		};
	});
}
