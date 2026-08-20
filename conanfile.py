from conan import ConanFile
from conan.tools.layout import basic_layout
from conan.tools.meson import MesonToolchain, Meson
from conan.tools.gnu import PkgConfigDeps

VULKAN_VERSION = "1.3.243.0"

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
        f"vulkan-headers/{VULKAN_VERSION}",
        f"vulkan-loader/{VULKAN_VERSION}",
        f"vulkan-validationlayers/{VULKAN_VERSION}",
        "tinygltf/2.5.0",
        "quill/10.0.1",
        "miniaudio/0.11.22",
        "fmt/11.2.0",
        # Docking is used by the shared editor workspace. Keep this on a
        # docking release and update the bundled GLFW/Vulkan backends with it.
        "imgui/1.92.8-docking",
        "gtest/1.15.0",
        "yaml-cpp/0.8.0",
        "magic_enum/0.8.2",
        "perlinnoise/3.0.0",
        "joltphysics/5.2.0",
    )

    def requirements(self):
        self.requires("libiconv/1.18", override=True)
        self.requires("stb/cci.20240531", override=True)

    def layout(self):
        # Share one Release dependency configuration between Meson build trees.
        basic_layout(self, build_folder="build")

    def generate(self):
        meson_toolchain = MesonToolchain(self)
        # Conan dependencies remain Release, but Krisp's build type is selected
        # independently when its Meson build directory is configured.
        meson_toolchain.buildtype = None
        meson_toolchain.b_ndebug = None
        meson_toolchain.generate()
        PkgConfigDeps(self).generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()
        # meson.install() installs it to wrong folder
        self.run(f"meson install -C {self.build_folder}")
