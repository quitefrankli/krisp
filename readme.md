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

```bash
conan build . -bf=build -pr=conan_clang_profile
build/bin/krisp
```

For initial setup these flags might come in handy
```bash
conan build . -bf=build -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True --build=missing -pr:a=conan_clang_profile
```

During development for debug build

```bash
meson setup build --reconfigure --buildtype=debug
meson compile -C build -j 6 krisp
build/applications/krisp/krisp
```

## Hot reload (Linux)

While `krisp` is running, press `Shift+R` to rebuild and reload the shared
library. Hot reload runs the equivalent of:

```bash
meson compile -C build -j 6 shared_lib
```

Each reload copies the module to a new versioned file in `build/bin` before
loading it, for example `libshared_lib1.so`. No separate reload script is
required.

## Testing

`meson test -C build`
