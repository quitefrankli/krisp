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
		"glm/0.9.8.5@bincrafters/stable",
		"vulkan-headers/1.2.172",
		"vulkan-loader/1.2.172",
		"vulkan-validationlayers/1.2.154.0",
		"stb/20190512@conan/stable",
		"tinyobjloader/1.0.6",
		"quill/1.6.3",
		"openal/1.21.1",
		"libsndfile/1.0.31",
		# "fmt/8.1.1", # already included by quill
		"imgui/1.85",
		"gtest/1.11.0",
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
		print('Vulkan-conan: building...')
		cmake = CMake(self, build_type=self.settings.build_type)
		cmake.configure()
		cmake.build(target='Vulkan')

		# generates shaders
		self.run('sh ../compile.sh')
		
