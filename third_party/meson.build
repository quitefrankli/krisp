sources = files(
	'ImGui/imgui_impl_vulkan.cpp',
	'ImGui/imgui_impl_glfw.cpp',
	'tiny_gltf/tiny_gltf.cpp',
	'stb_image/stb_image.cpp')

third_party_libs = static_library(
	'third_party_libs',
	sources,
	dependencies: conan_dependencies)

third_party_libs_dep = declare_dependency(
	include_directories: include_directories('.'),
	link_with: third_party_libs)