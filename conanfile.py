from conans import ConanFile, CMake

class vulkan_conan(ConanFile):
	settings = (
		"os",
		"compiler", 
		"build_type",
		"arch"
	)

	requires = (
		"glfw/3.3.2",
		"glm/0.9.9.8",
		"vulkan-headers/1.3.211.0",
		"vulkan-loader/1.3.211.0",
		"vulkan-validationlayers/1.3.211.0",
		"stb/20190512@conan/stable",
		"tinyobjloader/1.0.6",
		"quill/1.6.3",
		"openal/1.21.1",
		"libsndfile/1.0.31",
		# "fmt/8.1.1", # already included by quill
		"imgui/1.87",
		"gtest/1.8.1",
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
			# generates shaders
			self.run(f"sh {self.source_folder}/shader_compiler.sh")

			print('Vulkan-conan: building...')
			cmake.build(target='Vulkan')

	@staticmethod
	def process_option(option) -> str:
		if isinstance(option, bool):
			return '1' if option else '0'
		elif isinstance(option, str):
			return option.upper()
		else:
			raise RuntimeError(f'unsupported option type! {option}')
		
