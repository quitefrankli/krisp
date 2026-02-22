project('krisp', 'cpp', version: '0.1.0')

install_dir = meson.build_root()/'bin'
add_project_arguments(
	'-w', # Suppresses all build warnings for c++
	'-Werror=unused-variable',
	'-DPROJECT_TOP_LEVEL_SRC_DIR="@0@"'.format(meson.source_root()),
	'-DPROJECT_BUILD_DIR="@0@"'.format(meson.build_root()),
	'-DPROJECT_BIN_DIR="@0@"'.format(install_dir),
	'-DGLM_FORCE_DEPTH_ZERO_TO_ONE', # vulkan uses the z range -> [0, 1] as opposed to [-1, 1]
	'-DQUILL_FMT_EXTERNAL=ON',
	'-DGLM_ENABLE_EXPERIMENTAL',
	language: 'cpp')
if get_option('buildtype') == 'debug'
	add_project_arguments('-D_DEBUG', '-UNDEBUG', language: 'cpp')
endif

additional_include_directories = [
	include_directories('shared_code')
]

conan_dependencies = [
    dependency('glm', required: true),
    dependency('glfw3', required: true), # from glfw
    dependency('tinygltf', required: true),
	dependency('vulkan', required: true),
    dependency('quill', required: true),
    dependency('sndfile', required: true), # from libsndfile
    dependency('fmt', required: true),
    dependency('imgui', required: true),
    dependency('gtest', required: true),
    dependency('yaml-cpp', required: true),
    dependency('magic_enum', required: true),
	dependency('miniaudio', required: true),
	dependency('perlinnoise', required: true)
]

subdir('shaders')
subdir('third_party')
subdir('src')
subdir('test')
subdir('applications')