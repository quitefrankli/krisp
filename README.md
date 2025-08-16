# Requirements

* conan2: package manager
* meson: build system
* ninja: c++ build system
* clang: c++ compiler
* vulkan: graphics api + sdk
* glslc: shader compiler

## Building

```bash
conan build . -bf=build
build/bin/krisp
```

For initial setup these flags might come in handy
```bash
-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True --build=missing
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