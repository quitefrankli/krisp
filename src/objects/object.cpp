#include "object.hpp"
#include "serialization/serializer.hpp"

#include <algorithm>


Object::Object(Object&& other) noexcept :
	id(other.id),
	name(std::move(other.name)),
	bVisible(other.bVisible),
	transient_object(other.transient_object)
{}

void Object::serialize(Serializer& out) const
{
	out.write("type", serialization_type());
	out.write("id", id.get_underlying());
	out.write("name", name);
	out.write("visible", bVisible);
}

void Object::deserialize(const Deserializer& in)
{
	id = ObjectID(in.read<uint64_t>("id"));
	ObjectID::set_next_id(std::max(ObjectID::get_next_id(), id.get_underlying() + 1));
	name = in.read<std::string>("name");
	bVisible = in.read<bool>("visible");
}
