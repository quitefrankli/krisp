project('krisp', 'cpp', 'c', version: '0.1.0')

install_dir = meson.build_root()/'bin'
add_project_arguments(
	'-w', # Suppresses all build warnings for c++
	'-Werror=unused-variable',
	'-DGLM_FORCE_DEPTH_ZERO_TO_ONE', # vulkan uses the z range -> [0, 1] as opposed to [-1, 1]
	'-DQUILL_FMT_EXTERNAL=ON',
	'-DGLM_ENABLE_EXPERIMENTAL',
	# Match the SIMD feature set used by the Conan Jolt package.
	'-msse4.2', '-mavx2', '-mf16c', '-mfma', '-mlzcnt', '-mbmi',
	'-std=gnu++20',
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
    dependency('fmt', required: true),
    dependency('imgui', required: true),
    dependency('yaml-cpp', required: true),
    dependency('magic_enum', required: true),
	dependency('miniaudio', required: true),
	dependency('perlinnoise', required: true),
	dependency('joltphysics', required: true),
]

cc = meson.get_compiler('cpp')
ffmpeg_deps = [
	cc.find_library('avcodec', required: true),
	cc.find_library('avformat', required: true),
	cc.find_library('avutil', required: true),
	cc.find_library('swscale', required: true),
]

conan_dependencies += ffmpeg_deps

subdir('shaders')
subdir('shared_code')
subdir('third_party')
subdir('src')
subdir('tools')
subdir('runtime')
if get_option('build_tests')
	subdir('test')
endif
if get_option('build_applications')
	subdir('applications')
endif
