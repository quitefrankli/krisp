# Proposal

## Why

Internal applications and external applications consuming Krisp as a local Conan editable package need their own assets while sharing engine defaults. Staging those assets into runtime directories adds filesystem work and can duplicate large asset collections unnecessarily.

## What Changes

- Resolve app resources first, then shared engine defaults, through one resource-path module.
- Reference regular assets directly in their owning source directories and generated shaders/lighting directly in the engine build directory; remove development staging and its symlinks.
- Supply engine defaults centrally from the compiled library, with explicit overrides for custom layouts and Conan metadata describing editable locations.
- Separate resource/configuration lookup from Utility's logging and timing responsibilities; keep writable outputs separate from read-only assets.
- **BREAKING**: move resource lookup methods from `Utility` to `ResourcePaths` and replace `RuntimePaths::engine_runtime` with explicit `EnginePaths` fields.

## Capabilities

### New Capabilities

- `resource-path-resolution`: Shared app/default lookup, direct source/build paths, and editable-package integration for internal and external applications.

### Modified Capabilities

None. This is the project's first resource-path specification.

## Impact

Affects `src/resource_paths.*`, `src/utility.*`, `src/config.*`, resource consumers, bundled application setup, Meson runtime/build configuration, Conan dependency metadata, tests, and resource documentation. No new third-party dependency is required.

The implementation already exists in the working tree from the preceding discussion. This change records the agreed contract retrospectively; artifact creation does not authorize further code changes. Resource decoding, cache policy, asynchronous loading, and selective compiler optimization are outside this change.
