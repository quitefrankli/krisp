from conans import ConanFile, CMake

class vulkan_conan(ConanFile):
	settings = (
		"os",
		"compiler", 
		"build_type", 
		"arch"
	)
	requires = (
		"glfw/3.3.2@bincrafters/stable",
		"glm/0.9.8.5@bincrafters/stable",
		"vulkan-headers/1.2.172",
		"vulkan-loader/1.2.172",
		"vulkan-validationlayers/1.2.154.0"
	) 
	generators = (
		"cmake",
		"cmake_find_package"
	)
	# default_options = {"poco:shared": True, "openssl:shared": True}

	def imports(self):
		# copies all dll to bin folder (win)
		self.copy("*.dll", dst="bin", src="bin")
		# copies all dll to bin folder (macosx)
		self.copy("*.dylib*", dst="bin", src="lib")

	def build(self):
		cmake = CMake(self)
		cmake.configure()
		cmake.build()