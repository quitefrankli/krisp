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

Install Conan dependencies before using the run helper. They are built in
Release mode and shared by the separate Debug and Release Krisp build trees:

```bash
conan install . -pr=conan_clang_profile --build=missing
```

From the repository root, `./run [application] [--debug|--release]` configures
the selected build tree only when it has not been configured yet, builds the
selected application with six jobs, then launches it from the repository root.
The defaults are `sandbox` and Debug:

```bash
./run
./run sandbox --release
./run chess --debug
```

Conan's native file supplies compilers and dependency paths but deliberately
does not set Krisp's build type. To configure build trees manually, use:

```bash
meson setup build/debug --native-file build/conan/conan_meson_native.ini --buildtype=debug -Db_ndebug=false
meson setup build/release --native-file build/conan/conan_meson_native.ini --buildtype=release -Db_ndebug=true
```

After changing dependencies or build configuration, rerun `conan install` when
needed and reconfigure the affected tree by adding `--reconfigure` to the
corresponding `meson setup` command.

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
