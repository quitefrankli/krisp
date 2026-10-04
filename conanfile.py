import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain


VULKAN_VERSION = "1.3.243.0"


class KrispConan(ConanFile):
    name = "krisp"
    version = "0.1.0"
    package_type = "static-library"

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "build_applications": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "build_applications": True,
        "build_tests": True,
    }

    exports_sources = (
        "meson.build",
        "meson.options",
        "scripts/shader_compiler.sh",
        "scripts/stage_runtime.py",
        "configs/default.yaml",
        "src/*",
        "src/**/*",
        "applications/*",
        "applications/**/*",
        "shared_code/*",
        "third_party/*",
        "third_party/**/*",
        "shaders/*",
        "shaders/**/*",
        "resources/default/textures/skybox/*",
        "runtime/*",
        "runtime/**/*",
        "test/*",
        "test/**/*",
        "tools/*",
        "tools/**/*",
    )

    runtime_requirements = (
        "glfw/3.3.8",
        "glm/0.9.9.8",
        # Updates to Vulkan require an update to the version checked in the engine.
        f"vulkan-headers/{VULKAN_VERSION}",
        f"vulkan-loader/{VULKAN_VERSION}",
        f"vulkan-validationlayers/{VULKAN_VERSION}",
        "tinygltf/2.5.0",
        "quill/10.0.1",
        "miniaudio/0.11.22",
        "fmt/11.2.0",
        # The engine uses the docking branch and matching GLFW/Vulkan backends.
        "imgui/1.92.8-docking",
        "yaml-cpp/0.8.0",
        "magic_enum/0.8.2",
        "perlinnoise/3.0.0",
        "joltphysics/5.2.0",
    )

    def requirements(self):
        for requirement in self.runtime_requirements:
            self.requires(requirement, transitive_headers=True)
        self.requires("libiconv/1.18", override=True)
        self.requires("stb/cci.20240531", override=True)

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.15.0")

    def layout(self):
        # The normal checkout and Conan editable builds share these directories.
        self.folders.source = "."
        self.folders.build = "build/debug"
        self.folders.generators = "build/conan"

        self.cpp.source.includedirs = [
            "src",
            "shared_code",
            "third_party",
        ]
        self.cpp.build.libdirs = ["src", "third_party"]
        self.cpp.build.builddirs = ["runtime"]
        self.cpp.package.includedirs = ["include/krisp"]
        self.cpp.package.libdirs = ["lib"]
        self.cpp.package.builddirs = ["share/krisp"]

    def generate(self):
        meson_toolchain = MesonToolchain(self)
        # Build type and NDEBUG are set explicitly by Meson, independently from
        # the Release Conan dependency profile.
        meson_toolchain.buildtype = None
        meson_toolchain.b_ndebug = None
        meson_toolchain.generate()
        PkgConfigDeps(self).generate()

    def build(self):
        if str(self.settings.build_type) != "Debug":
            raise ConanInvalidConfiguration("Krisp packages are Debug-only during the initial migration")

        coredata = os.path.join(self.build_folder, "meson-private", "coredata.dat")
        reconfigure = " --reconfigure" if os.path.isfile(coredata) else ""
        self.run(
            "meson setup"
            f"{reconfigure}"
            f' --native-file "{self.generators_folder}/conan_meson_native.ini"'
            " --buildtype=debug -Db_ndebug=false --prefix=/ --libdir=lib"
            f" -Dbuild_applications={str(self.options.build_applications).lower()}"
            f" -Dbuild_tests={str(self.options.build_tests).lower()}"
            f' "{self.build_folder}" "{self.source_folder}"'
        )
        targets = ["krisp_libs", "third_party_libs", "krisp_runtime"]
        if self.options.build_applications:
            targets.append("krisp")
        if self.options.build_tests:
            targets.append("krisp_tests")
        self.run(f'meson compile -C "{self.build_folder}" -j 6 ' + " ".join(targets))

    def package(self):
        # Meson installs relative to /; DESTDIR stages those files in the
        # Conan package without writing to the machine root.
        self.run(f'DESTDIR="{self.package_folder}" meson install -C "{self.build_folder}" --no-rebuild --tags engine')

    def package_id(self):
        # These control extra build targets, not the installed engine library.
        self.info.options.rm_safe("build_applications")
        self.info.options.rm_safe("build_tests")

    def package_info(self):
        self.cpp_info.libs = ["krisp_libs", "third_party_libs"]
        self.cpp_info.includedirs = ["include/krisp"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.builddirs = ["share/krisp"]
        self.cpp_info.defines = [
            "GLM_FORCE_DEPTH_ZERO_TO_ONE",
            "GLM_ENABLE_EXPERIMENTAL",
            "QUILL_FMT_EXTERNAL=ON",
        ]
        if str(self.settings.build_type) == "Debug":
            self.cpp_info.defines.append("_DEBUG")
        self.cpp_info.cxxflags = [
            "-msse4.2",
            "-mavx2",
            "-mf16c",
            "-mfma",
            "-mlzcnt",
            "-mbmi",
        ]
        self.cpp_info.system_libs = [
            "avcodec",
            "avformat",
            "avutil",
            "swscale",
            "dl",
            "pthread",
        ]
        self.cpp_info.set_property("pkg_config_name", "krisp")
