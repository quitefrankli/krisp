#include "shape_factory.hpp"
#include "shapes/shapes.hpp"


Shape ShapeFactory::generate_cube()
{
	Shape new_shape;

	auto add_face = [&](const glm::quat& rotator)
	{
		const int index_offset = new_shape.vertices.size();
		Shapes::Square front;
		// glm::mat4 idt(1.0f);
		// glm::translate(idt, )
		front.translate_vertices(glm::vec3(0.0f, 0.0f, 0.5f));
		front.transform_vertices(glm::mat4_cast(rotator));
		new_shape.vertices.insert(new_shape.vertices.end(),
						std::make_move_iterator(front.vertices.begin()),
						std::make_move_iterator(front.vertices.end()));
		for (auto index : front.indices)
		{
			new_shape.indices.push_back(index + index_offset);
		}
	};
	
	new_shape.vertices.reserve(6*6);
	add_face(glm::quat{});
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)));

	// deduplicate_vertices(); // TODO: do this
	new_shape.generate_normals();

	return new_shape;
}

Shape ShapeFactory::generate_sphere()
{
	Shape new_shape;

	Shapes::Sphere sphere(15);
	new_shape.vertices = std::move(sphere.vertices);
	new_shape.indices = std::move(sphere.indices);

	// deduplicate_vertices(); // TODO: do this
	new_shape.generate_normals();

	return new_shape;
}