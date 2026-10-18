# Tasks

This is a retrospective checklist of the implementation completed earlier in this conversation. Checked items are grounded in the existing source, tests, documentation, and the preceding build/consumer verification; no implementation or build rerun was performed while authoring this change.

## 1. Resource-path interface and lookup

- [x] 1.1 Introduce independent engine resource, shader, default-configuration, and lighting locations alongside app paths; verify `resource_paths.hpp` and the direct source/build path test in `test/common_tests.cpp`.
- [x] 1.2 Extract lookup from Utility, preserving app precedence, engine fallback, relative-name rejection, and relative unique enumeration; verify ResourcePaths implementation and resource tests in `test/common_tests.cpp`.
- [x] 1.3 Initialize ResourcePaths through Config and retain Utility logging/timing and writable-save helpers; verify initialization in `src/config.cpp` and the existing config/save-path test.
- [x] 1.4 Document resource ownership, lookup, explicit overrides, and the new interface in `docs/RESOURCES.md`; verify the document matches the configured path fields.

## 2. Direct development assets and editable integration

- [x] 2.1 Generate engine path defaults centrally and migrate bundled application callers; verify Meson path configuration, migrated entry points, and successful Debug builds of all bundled applications from the preceding implementation turn.
- [x] 2.2 Remove development staging and retain generation dependencies for shaders/lighting; verify the deleted staging script, runtime target dependency list, and direct source/build path test.
- [x] 2.3 Export all four editable engine locations through Conan pkg-config metadata; verify the preceding separate consumer installation and queries returned source/build paths for all four variables.
- [x] 2.4 Keep package installation separate from development lookup, with checkout-independent defaults when development paths are disabled; verify Meson installation rules, Conan build option, ResourcePaths fallback, and the inspected package install plan. A full package creation was not run.
- [x] 2.5 Keep writable test fixtures outside engine source assets; verify `test/test_main.cpp` configures an isolated fixture root beneath `build/debug/test-data`.
- [x] 2.6 Document editable locations and the removal of staging in `docs/CONAN.md` and `docs/PERFORMANCE.md`; verify these notes agree with the metadata and build configuration.

## 3. Integration verification

- [x] 3.1 Run `meson test -C build/debug -j 6`; both Meson test suites passed in the preceding implementation turn, and their existing test log reports OK.
- [x] 3.2 Build/link a separate neutral consumer through Conan editable metadata and run it from outside both checkouts; the preceding consumer check passed engine fallback, app override, configuration, and shader lookup without consumer engine-path macros.

## Workflow follow-up

- Review this captured contract against the existing implementation before promoting it to the main specification.
- Archive the change only when requested. No external application migration, commit, or archive is performed by this documentation task.
