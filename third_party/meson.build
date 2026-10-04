sources = files(
	'ImGui/imgui_impl_vulkan.cpp',
	'ImGui/imgui_impl_glfw.cpp',
	'tiny_gltf/tiny_gltf.cpp',
	'stb_image/stb_image.cpp',
	'MikkTSpace/mikktspace.c')

third_party_libs = static_library(
	'third_party_libs',
	sources,
	install: true,
	install_tag: 'engine',
	dependencies: conan_dependencies)

third_party_libs_dep = declare_dependency(
	include_directories: include_directories('.'),
	link_with: third_party_libs)

third_party_header_files = files(
	'ImGui/imgui_impl_glfw.h',
	'ImGui/imgui_impl_vulkan.h',
	'MikkTSpace/mikktspace.h')

install_data(third_party_header_files,
	install_dir: 'include/krisp',
	preserve_path: true,
	install_tag: 'engine')
