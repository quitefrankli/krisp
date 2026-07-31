# Krisp

A high-performance C++/Vulkan game engine.

# Requirements

* conan2: package manager `mamba: conan`
* meson: build system `install meson`
* cmake: c++ build system `install cmake`
* ninja: c++ build system `install ninja` or `install ninja-build`
* clang: c++ compiler `install clang-17` + `clang-tools-17`
* glslc: shader compiler `mamba: shaderc`
* vulkan: graphics api + sdk -> check if it's available via `vulkaninfo`
* validation layers: `install vulkan-validationlayers`
* FFmpeg development libraries: `avcodec`, `avformat`, `avutil`, and `swscale`
  (for example, Debian/Ubuntu packages `libavcodec-dev`, `libavformat-dev`,
  `libavutil-dev`, and `libswscale-dev`)

Video recording additionally requires an FFmpeg installation with the
`libx264` encoder enabled.

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

If Conan must install missing system packages during initial setup, use:

```bash
conan install . -pr=conan_clang_profile --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
```

## Testing

`meson test -C build -j 6`
