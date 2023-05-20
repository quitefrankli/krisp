from conans import ConanFile, CMake

class CustomWrapper:
	def __init__(self, conanfile: ConanFile):
		self.conanfile = conanfile

	def __enter__(self):
		self.conanfile.should_build = True

	def __exit__(self, exc_type, exc_val, exc_tb):
		self.conanfile.should_build = False

class vulkan_conan(ConanFile):
	settings = (
		"os",
		"compiler", 
		"build_type",
		"arch"
	)

	requires = (
		"glfw/3.3.8",
		"glm/0.9.9.8",
		# updates to vulkan requires updates to version in code
		# checkout GraphicsEngineInstance
		"vulkan-headers/1.3.211.0",
		"vulkan-loader/1.3.211.0",
		"vulkan-validationlayers/1.3.211.0",
		"stb/20190512@conan/stable",
		"tinygltf/2.5.0",
		"quill/1.6.3",
		"openal/1.21.1",
		"libsndfile/1.0.31",
		# "fmt/8.1.1", # already included by quill
		"imgui/1.87", # also update backend under third_party
		"gtest/1.8.1",
		"yaml-cpp/0.7.0",
		"magic_enum/0.8.2"
	) 

	generators = (
		"cmake",
		"cmake_find_package"
	)

	def imports(self):
		# copies all dll to bin folder (win)
		self.copy("*.dll", dst="bin", src="bin")
		# copies all dll to bin folder (macosx)
		self.copy("*.dylib*", dst="bin", src="lib")

	def build(self):
		cmake = CMake(self)
		if self.should_configure:
			print('Vulkan-conan: configuring...')
			for key, val in self.options.items():
				cmake.definitions[key.upper()] = self.process_option(val)

			# some compilers (i.e. msvc) don't have specific build_type at configure stage
			cmake.definitions['CMAKE_BUILD_TYPE'] = self.settings.build_type
			cmake.configure()

		if self.should_build:
			print('Vulkan-conan: building...')
			cmake.build(target='Vulkan')

		if self.should_test:
			with CustomWrapper(self):
				cmake.build(target='Vulkan')
				cmake.build(target='unittests')
				
			print('Vulkan-conan: testing...')
			cmake.test(output_on_failure=True)

	@staticmethod
	def process_option(option) -> str:
		if isinstance(option, bool):
			return '1' if option else '0'
		elif isinstance(option, str):
			return option.upper()
		else:
			raise RuntimeError(f'unsupported option type! {option}')
		
