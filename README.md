# Requirements

* conan2: package manager `mamba: conan`
* meson: build system `meson`
* ninja: c++ build system `ninja`
* clang: c++ compiler `clang` + `clang-tools`
* vulkan: graphics api + sdk -> check fi it's available via `vulkaninfo`
* glslc: shader compiler `mamba: shaderc`

## Building

```bash
conan build . -bf=build
build/bin/krisp
```

For initial setup these flags might come in handy
```bash
conan build . -bf build -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True --build=missing -pr:a=conan_clang_profile
```

During development for debug build
```bash
meson setup build --reconfigure --buildtype=debug
ninja -C build install
```

## Testing

```bash
ninja -C build test
```