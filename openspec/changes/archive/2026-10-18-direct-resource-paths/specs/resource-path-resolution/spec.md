# Spec Delta

## Purpose

Lets applications use their own assets alongside shared engine defaults while resolving both directly from development source and build locations. It also defines how external editable consumers discover and override engine paths.

## ADDED Requirements

### Requirement: Applications can resolve application-owned assets
An application SHALL be able to resolve its own regular assets whether it is built inside the engine checkout or consumes the engine as an external editable package.

#### Scenario: Internal application resolves its own asset
- **WHEN** an internal application requests a file present in its resource root
- **THEN** resolution returns that application-owned file

#### Scenario: External editable consumer resolves its own asset
- **WHEN** an external application requests a file present in its resource root
- **THEN** resolution returns that application-owned file independently of the engine checkout location

### Requirement: Application assets take precedence over engine defaults
When a requested resource exists in both application and engine resource roots, resolution SHALL return the application resource; when only the engine resource exists, resolution SHALL return the engine resource.

#### Scenario: Application resource overrides a shared default
- **WHEN** the same relative resource name exists in both roots
- **THEN** resolution returns the application-owned file

#### Scenario: Engine resource supplies a missing application asset
- **WHEN** a relative resource name is absent from the application root and present in the engine root
- **THEN** resolution returns the engine-owned file

### Requirement: Missing resources produce an error
Regular asset lookup SHALL report an error when the requested asset is absent from both application and engine resource roots.

#### Scenario: Resource is absent from both roots
- **WHEN** an application requests a name that exists in neither root
- **THEN** lookup throws an error identifying that the resource could not be found

### Requirement: Development resolves files from their owning directories
In development, regular assets SHALL be read from their owning source directories and generated engine data SHALL be read from engine build output directories, without copying regular assets into a runtime tree or creating development symlinks for them.

#### Scenario: Asset is edited in place during development
- **WHEN** a regular asset is changed at its owning source path
- **THEN** subsequent file access through its resolved path observes the changed file without a staging or symlink refresh step

#### Scenario: Generated engine data is resolved from the build tree
- **WHEN** development code resolves generated shaders or environment lighting
- **THEN** the returned location is in the engine build output tree

### Requirement: Engine paths do not depend on the working directory
In local and editable development, default engine resource and configuration paths SHALL resolve from the engine source location, and generated shader and lighting paths SHALL resolve from the engine build location, regardless of the consumer process's current working directory.

#### Scenario: Consumer starts outside the engine checkout
- **WHEN** a consumer launches with a working directory unrelated to the engine source and build directories
- **THEN** default engine paths still identify the corresponding engine source and build locations

### Requirement: External editable consumers receive engine defaults automatically
An external application consuming a built local editable engine package SHALL receive default engine resource, shader, configuration, and lighting paths without supplying engine checkout paths or engine path macros.

#### Scenario: Consumer configures only its own resource root
- **WHEN** an external editable consumer uses engine-provided path defaults and configures its own resource root
- **THEN** engine fallback and generated-data lookup use the shared engine source/build locations without further engine path configuration

### Requirement: Editable package metadata exposes all engine locations
Editable package pkg-config metadata SHALL expose the four variables `krisp_resources`, `krisp_shaders`, `krisp_default_config`, and `krisp_environment_lighting` for the shared engine locations.

#### Scenario: Consumer queries editable location variables
- **WHEN** a consumer queries editable package metadata for engine locations
- **THEN** all four variables resolve to their corresponding engine resource, shader, default configuration, and environment lighting paths

### Requirement: Consumers can override engine paths
Runtime configuration SHALL allow consumers to provide independent paths for engine resources, shaders, default configuration, and environment lighting.

#### Scenario: Consumer provides a custom engine layout
- **WHEN** a consumer configures custom locations for the four engine path categories
- **THEN** engine lookups use those configured locations

### Requirement: Application shaders and configuration can override defaults
Application-owned shaders SHALL take precedence over engine shaders with the same relative name, and an existing application configuration SHALL take precedence over the engine default configuration.

#### Scenario: Application supplies a shader override
- **WHEN** a shader exists in both the application shader root and engine shader root
- **THEN** shader lookup returns the application shader

#### Scenario: Application supplies a configuration override
- **WHEN** a requested configuration file exists in the application's configuration location
- **THEN** configuration lookup returns that application file

### Requirement: Writable outputs remain application-local
Default writable application data and user configuration locations SHALL be scoped to the application and SHALL remain separate from read-only engine and application asset paths. Explicit writable-location overrides SHALL be supported.

#### Scenario: Two applications resolve writable paths
- **WHEN** two applications request their writable data locations
- **THEN** each receives an application-specific location outside the shared read-only asset roots

#### Scenario: Application requests a user configuration output
- **WHEN** an application requests the path for a user configuration file
- **THEN** the returned path is under that application's configuration location

### Requirement: Resource names are lexically relative
Resource, shader, and configuration lookup SHALL reject empty names, absolute paths, and names containing a `..` path component before combining a name with a root.

#### Scenario: Invalid resource names are rejected
- **WHEN** lookup receives an empty name, an absolute name, or a name containing a parent-directory component
- **THEN** lookup reports an error and does not return a resource path

#### Scenario: Nested relative resource name is accepted
- **WHEN** lookup receives a nonempty relative name without a parent-directory component
- **THEN** lookup returns a path beneath the selected root when the named file exists

### Requirement: Resource enumeration returns relative unique names
Resource enumeration SHALL return names relative to their respective resource category and SHALL include a matching name only once when it exists in both application and engine roots.

#### Scenario: Matching resources are present in both roots
- **WHEN** enumeration finds the same relative filename in application and engine resource roots
- **THEN** the result contains that relative filename once

#### Scenario: Enumerated name is independent of root location
- **WHEN** enumeration returns a nested resource
- **THEN** the result is a relative filename and does not contain either root path
