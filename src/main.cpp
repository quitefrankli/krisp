#include "game_engine.hpp"
#include "maths.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <thread>
#include <iomanip>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
// #include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>


quill::Logger* logger;

void pretty_print(glm::mat4 mat)
{
	std::cout << std::setprecision(2) << std::fixed << '\n';
	mat = glm::transpose(mat);
	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			std::cout << std::setw(7) << mat[row][col];
		}
		putchar('\n');
	}
}
// void pretty_print(glm::mat4 mat)
// {
// 	std::cout << std::setprecision(2) << std::fixed << '\n';
// 	mat = glm::transpose(mat);
// 	for (int row = 0; row < 4; row++)
// 	{
// 		for (int col = 0; col < 4; col++)
// 		{
// 			std::cout << std::setw(7) << mat[row][col];
// 		}
// 		putchar('\n');
// 	}
// }

std::string RELATIVE_BINARY_PATH;

int main(int argc, char* argv[]) {
	// glm::vec3 axis(0.0f, 1.0f, 0.0f);
	// float magnitude = Maths::deg2rad(90.0f);
	// glm::quat quaternion = glm::angleAxis(magnitude, axis);
	// glm::mat4 mat = glm::mat4_cast(quaternion);
	// glm::vec4 vec(0.0f, 0.0f, 1.0f, 0.0f);
	// std::cout << "expected\n";
	// std::cout << glm::to_string(mat * vec) << '\n';
	// // pretty_print(mat);

	// std::cout << "actual\n";
	// // glm::vec4 result = quaternion * vec * glm::inverse(quaternion);
	// glm::vec4 result = quaternion * vec;
	// std::cout << glm::to_string(result) << '\n';

	// auto axis2 = quaternion * glm::vec3(1.0f, 0.0f, 0.0f);
	// auto quat2 = glm::angleAxis(magnitude, axis2);
	// result = quat2 * result;
	// std::cout << glm::to_string(result) << '\n';
	// return 0;

	RELATIVE_BINARY_PATH = argv[0];

	auto file_handler = quill::file_handler("log.log", "a");
	logger = quill::create_logger("MAIN", file_handler);
	file_handler->set_pattern(
		QUILL_STRING("%(ascii_time): %(message)"),
		"%D %H:%M:%S.%Qus",
		quill::Timezone::LocalTime
	);
	// quill::start(); // this will consume CPU cycles

	// seems like glfw window must be on main thread otherwise it wont work, therefore engine should always be on its own thread
	GameEngine engine;

    try {
		engine.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
	} catch (...) {
		std::cout << "Exception Thrown!\n";
	}

    return EXIT_SUCCESS;
}