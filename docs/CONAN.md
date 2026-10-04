# Building Krisp with Conan

Krisp can be built as the static library package `krisp/0.1.0`. Apps consume it
with Conan 2 and Meson. The package contains headers, static libraries, and
runtime data under `share/krisp`; FFmpeg and Vulkan drivers remain system
requirements.

## Develop the engine with an editable package

Conan editable mode maps `krisp/0.1.0` to this checkout, so engine edits do
not require new cached packages:

```sh
conan editable add /path/to/krisp
cd /path/to/krisp
conan install . -pr conan_clang_profile --build=missing --no-remote
meson setup build/debug --reconfigure \
  --native-file build/conan/conan_meson_native.ini \
  --buildtype=debug -Db_ndebug=false
meson compile -C build/debug -j 6 krisp
```

The editable runtime directory is `build/debug/runtime`; packaged runtime data
lives under `share/krisp`. Reinstall and reconfigure the engine when dependency
metadata changes. Editable registration applies to this package reference in
the active Conan home.

Remove the editable mapping with
`conan editable remove --refs=krisp/0.1.0` when a cached package is required.
Application setup and integration instructions belong in consumer repositories.

## Create a checkpoint package

Build an intentional checkpoint when another developer needs an immutable
package. Package builds are Debug and exclude the bundled app and tests:

```sh
conan create . -pr:h conan_clang_profile -pr:b conan_clang_profile \
  -o '&:build_applications=False' -o '&:build_tests=False' \
  --build=missing --no-remote
```

Consumers use the same `krisp/0.1.0` reference; Conan resolves it from the
editable checkout during co-development and from the cache when no editable is
registered. Create a new package only for a deliberate checkpoint. The package
version stays at `0.1.0` during this initial migration; record the source Git
revision alongside each checkpoint rather than assigning a version per edit.
For reproducible distribution, pin the checkpoint's Conan recipe/package
revisions in a consumer lockfile. Editable development intentionally follows the
working checkout instead.

## Dependencies and platform notes

All Conan commands use the root `conan_clang_profile`. It selects Debug for
the root project and `krisp/*`, and Release for other dependencies. In
particular, Conan dependencies remain Release while Meson compiles the Krisp
checkout as Debug using the explicit setup command above. Meson's
`--buildtype=debug -Db_ndebug=false` options own the compiler configuration for
that checkout.

FFmpeg libraries (`avcodec`, `avformat`, `avutil`, and `swscale`) must be
installed by the system. Linux `dl` and `pthread` are linked by the package.
Vulkan headers, loader, and validation layers are Conan dependencies; a
compatible Vulkan driver must be installed on the host.

The examples use `--no-remote` to reuse cached dependencies. Omit it when setting
up a machine that needs to download dependencies. Engine package builds currently
support Debug on Linux with the root profile; the AVX2/SSE feature flags
match the existing Jolt build and are exported to consumers.
