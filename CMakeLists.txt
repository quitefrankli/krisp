cmake_minimum_required(VERSION 3.20)

# set the project name
project(Vulkan)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ version")
set(CMAkE_CXX_STANDARD_REQUIRED True CACHE STRING "is standard required?")
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

if(MSVC)
	message("Detected Windows OS")
	# for MSVC this will set the limit of max simultaneous TU compiles
	add_definitions(/MP12)
elseif(APPLE)
	message("Detected APPLE")
	# add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
	set(CMAKE_SKIP_RPATH False)
	add_compile_options(-Werror -Wno-error=deprecated-declarations)
else()
	message(FATAL_ERROR "OS is not supported!")
endif()

add_definitions(-DPROJECT_TOP_LEVEL_SRC_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DPROJECT_BUILD_DIR="${CMAKE_BINARY_DIR}")
add_definitions(-DPROJECT_BIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE) # vulkan uses the z range -> [0, 1] as opposed to [-1, 1]
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

if(NOT ENABLE_COMPILER_WARNINGS)
	# MSVC warnings https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=msvc-170
	if(MSVC)
		add_definitions(/W1)
	else()
		add_definitions(-w)
	endif()
endif()

if(DISABLE_SLEEP)
	add_definitions(-DDISABLE_SLEEP)
endif()

file(GLOB_RECURSE 
	 VulkanSources 
	 ${CMAKE_SOURCE_DIR}/src/*.cpp
	 ${CMAKE_SOURCE_DIR}/third_party/*.cpp)
# remove main.cpp from VulkanSources
list(FILTER VulkanSources EXCLUDE REGEX "main.cpp")

set(VulkanIncludes 
	${CMAKE_SOURCE_DIR}/src/ 
	${CMAKE_SOURCE_DIR}/third_party/ 
	${CMAKE_SOURCE_DIR}/shared_lib/src/
	${CMAKE_SOURCE_DIR}/shared_code/)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

add_library(VulkanLibs ${VulkanSources})
# public keyword means that stuff that depends on VulkanLibs will also inherit this include
target_include_directories(VulkanLibs PUBLIC ${VulkanIncludes})

add_executable(Vulkan ${CMAKE_SOURCE_DIR}/src/main.cpp) # bit annoying but seems we need to include at least 1 source file
# set_target_properties(Vulkan PROPERTIES BUILD_RPATH ${CMAKE_BINARY_DIR}/bin)
target_link_libraries(Vulkan VulkanLibs ${CONAN_LIBS})

if(MSVC)
	add_subdirectory(applications)
	add_subdirectory(shared_lib)
endif()
enable_testing()
add_subdirectory(test)

# omits the annoying pdb debug symbol missing console spam
if(MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

message("Configuring for ${CMAKE_BUILD_TYPE} mode")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(Vulkan PRIVATE DEBUG_MODE=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	target_compile_definitions(Vulkan PRIVATE DEBUG_MODE=0)
else()
	message(FATAL_ERROR "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} is not supported!")
endif()

# build shaders
file(
	GLOB_RECURSE 
	SHADER_FILES 
	${CMAKE_SOURCE_DIR}/shaders/*
	${CMAKE_SOURCE_DIR}/shared_code/*.txt
	${CMAKE_SOURCE_DIR}/shared_code/*.glsl)

set(DUMMY_FILE ${CMAKE_BINARY_DIR}/shaders/dummy)

add_custom_command(
	OUTPUT ${DUMMY_FILE}
	COMMENT "Cmake detected changes in shaders, rebuilding shaders..."
	COMMAND bash ${CMAKE_SOURCE_DIR}/shader_compiler.sh && touch ${DUMMY_FILE}
	DEPENDS ${SHADER_FILES})
	
add_custom_target(shaders DEPENDS ${DUMMY_FILE})
add_dependencies(VulkanLibs shaders)

# target_link_libraries(Vulkan ${CONAN_LIBS} shared_lib) # this links a start-time dynamic lib when we actually want a run-time dynamic lib
# target_include_directories(Vulkan PRIVATE "src/" "shared_lib/src/")