#include "object.hpp"
#include "serialization/serializer.hpp"

#include <algorithm>
#include <limits>


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
	const uint64_t restored_id = in.read<uint64_t>("id");
	if (restored_id == std::numeric_limits<uint64_t>::max())
		throw SerializationError("Cannot advance ObjectID counter beyond uint64 maximum at " + in.path());
	id = ObjectID(restored_id);
	ObjectID::set_next_id(std::max(ObjectID::get_next_id(), restored_id + 1));
	name = in.read<std::string>("name");
	bVisible = in.read<bool>("visible");
}
