cmake_minimum_required(VERSION 3.20)

# set the project name
project(rts)

file(GLOB_RECURSE RTS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(rts ${RTS_SOURCES})
target_include_directories(rts PRIVATE ${VulkanIncludes})
target_link_libraries(rts VulkanLibs ${CONAN_LIBS})

# omits the annoying pdb debug symbol missing console spam
if(MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
endif()