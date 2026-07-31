# Krisp

[A high-performance C++/Vulkan game engine](https://nabicat.site/hammock/krisp/krisp)

# Requirements

* conan2: package manager `mamba: conan`
* meson: build system `install meson`
* cmake: c++ build system `install cmake`
* ninja: c++ build system `install ninja` or `install ninja-build`
* clang: c++ compiler `install clang-17` + `clang-tools-17`
* glslc: shader compiler `mamba: shaderc`
* vulkan: graphics api + sdk -> check if it's available via `vulkaninfo`
* validation layers: `install vulkan-validationlayers`

## Building

Use the following sequence for development. It installs missing Conan
dependencies with the Clang profile, configures a debug build using Conan's
generated Meson native file, and builds only the `krisp` target:

```bash
conan install . -pr=conan_clang_profile --build=missing
meson setup build --reconfigure --native-file build/conan/conan_meson_native.ini --buildtype=debug
meson compile -C build -j 6 krisp
```

Run the application with:

```bash
build/applications/krisp/krisp
```

(during initial setup these flags can be useful -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True)

## Testing

`meson test -C build -j 6`
