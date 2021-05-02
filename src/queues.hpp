/*
	Everything in VUlkan uses a queue, for piping commands around etc
*/

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily; // as in present image to the window surface

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};