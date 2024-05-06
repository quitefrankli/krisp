#include "test_helper.hpp"

#include <shared_data_structures.hpp>
#include <renderable/material.hpp>

#include <gtest/gtest.h>


TEST(Basics, MaterialMoveCtor)
{
	ColorMaterial material;
	material.data.shininess = 2.5f;

	std::unique_ptr<Material> ptr = std::make_unique<ColorMaterial>(std::move(material));
	auto& material2 = static_cast<ColorMaterial&>(*ptr);

	ASSERT_EQ(material2.data.shininess, 2.5f);
}
