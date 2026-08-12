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

Conan dependencies are always built in Release mode and shared by the separate
Debug and Release Krisp build trees. Install or update them with:

```bash
conan install . -pr=conan_clang_profile --build=missing
```

Configure each Krisp build tree once:

```bash
meson setup build/debug --native-file build/conan/conan_meson_native.ini --buildtype=debug -Db_ndebug=false
meson setup build/release --native-file build/conan/conan_meson_native.ini --buildtype=release -Db_ndebug=true
```

The explicit `b_ndebug` values override the Release setting in Conan's native
file for Krisp's own compilation. Build and run either configuration with:

```bash
meson compile -C build/debug -j 6 krisp
build/debug/applications/krisp/krisp

meson compile -C build/release -j 6 krisp
build/release/applications/krisp/krisp
```

After changing dependencies or build configuration, rerun `conan install` when
needed and reconfigure the affected tree by adding `--reconfigure` to its
`meson setup` command.

If Conan must install missing system packages during initial setup, use:

```bash
conan install . -pr=conan_clang_profile --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
```

## Testing

```bash
meson compile -C build/debug -j 6 krisp_tests
meson test -C build/debug -j 6
```

## Documentation

Additional documentation and design notes are available in [`docs/`](docs/).
