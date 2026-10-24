# Spec Delta

## Purpose

Provide repeatable, exact pixel checks of production rendering on the current GPU and driver, with approved baselines and optional visual evidence for reviewing changes.

## ADDED Requirements

### Requirement: Capture a fixed production rendering snapshot
The suite SHALL capture an identified immutable rendering snapshot after presentation processing and before UI, excluding diagnostic overlays, with fixed fixture poses and settings.

#### Scenario: Capture selected fixture
- **WHEN** a selected case is captured
- **THEN** its identity, fixed pose, effective settings, and production-rendered image are associated with the same immutable snapshot

### Requirement: Run on the current graphics setup
The suite SHALL use the current GPU and driver through the production renderer without provisioning or requiring a pinned software renderer. Missing graphics or required verified assets SHALL produce an actionable nonpassing result rather than substitute rendering or skip a requested case.

#### Scenario: A prerequisite is unavailable
- **WHEN** a requested case cannot access graphics or verified required assets
- **THEN** it records the missing prerequisite and the run returns nonzero

### Requirement: Isolate rendering from the desktop
The suite SHALL render on one private virtual framebuffer display per run, using the current hardware GPU. Worker windows SHALL NOT appear on the user's desktop or take focus. Missing virtual-display prerequisites SHALL fail explicitly without falling back to the user's display, and the private display SHALL be cleaned up after execution.

#### Scenario: Run unattended without desktop windows
- **WHEN** the suite runs while the user is using their desktop
- **THEN** its workers share a private virtual display and record the actual selected GPU
- **AND** completion or failure cleans up that display without affecting the desktop

#### Scenario: Virtual display cannot start
- **WHEN** virtual-display dependencies are missing or startup fails
- **THEN** the run preserves an actionable nonpassing diagnostic and does not launch visible workers

### Requirement: Cover focused rendering cases
The suite SHALL provide focused cases for colour and presentation exposure and tone mapping; PBR factor combinations; textures and normal maps; direct and environment lighting; shadows with multiple lights; depth, sidedness, alpha mask and blend; transforms and skinning; and one fixed large textured Sponza view using verified prepared assets.

#### Scenario: Run the selected case set
- **WHEN** a run selects the rendering suite
- **THEN** it includes the focused cases and fixed Sponza view with their prepared asset identities

### Requirement: Compare decoded images exactly
The verdict SHALL require equal image dimensions and exact equality of every decoded RGBA pixel; PNG compression and metadata SHALL NOT affect comparison, and no tolerance, mask, perceptual, or numerical alternate verdict is permitted.

#### Scenario: Compare images with different PNG encoding
- **WHEN** candidate and baseline decode to identical RGBA pixels and dimensions but differ in PNG compression or metadata
- **THEN** the image comparison passes

#### Scenario: Detect a changed pixel
- **WHEN** dimensions differ or any decoded RGBA pixel differs
- **THEN** the image comparison fails

### Requirement: Require fresh-process repeatability
Every selected case SHALL be captured at least twice in fresh processes per run, and every capture SHALL agree exactly with the others; any disagreement makes the case nonpassing.

#### Scenario: Captures disagree
- **WHEN** fresh-process captures of a selected case differ in dimensions or any decoded RGBA pixel
- **THEN** that case is nonpassing

### Requirement: Check baseline environment and content compatibility
The suite SHALL record GPU and driver environment, effective case settings, fixture and asset identities, and source provenance; a GPU or driver mismatch or changed fixture, asset, or effective settings identity SHALL make a baseline incompatible and nonpassing.

#### Scenario: Environment or fixture identity differs
- **WHEN** a run's GPU or driver, fixture or asset identity, or effective settings identity is incompatible with the approved baseline
- **THEN** the case is nonpassing as incompatible

#### Scenario: Source revision differs
- **WHEN** source provenance differs while the GPU and driver, fixture and asset identities, and effective settings remain compatible
- **THEN** the source revision difference alone does not make the baseline incompatible

### Requirement: Keep approved baselines unchanged during runs
Fidelity runs SHALL read approved baselines without modifying or replacing them.

#### Scenario: Run against an approved baseline
- **WHEN** a fidelity run compares candidate captures with an approved baseline
- **THEN** the baseline content remains unchanged

### Requirement: Require specific approval for baseline changes
Initial baseline creation or replacement SHALL require explicit user approval of the exact candidate cases, image digests, and environment. Candidates SHALL have complete, repeatable captures compatible with their destination profile; matching an existing baseline is not required. Modified or stale evidence SHALL be rejected.

#### Scenario: Approve a reviewed candidate
- **WHEN** the user approves the exact current complete and repeatable candidate cases, digests, and environment
- **THEN** only that candidate evidence may be used to create or replace the baseline

#### Scenario: Candidate changed after review
- **WHEN** candidate evidence changes after approval or its cases or environment do not match the approved evidence
- **THEN** baseline creation or replacement is rejected

#### Scenario: Candidate evidence is invalid
- **WHEN** candidate evidence is incomplete, incompatible with its destination profile, or not repeatable
- **THEN** it cannot be approved for baseline creation or replacement

#### Scenario: Initial baseline or intentional appearance change
- **WHEN** complete and repeatable candidates have no existing baseline or differ from an existing baseline
- **THEN** those candidates remain nonpassing for comparison but can be presented for explicit user approval

### Requirement: Report missing baselines and aggregate verdicts honestly
The suite SHALL report a missing baseline as approval-required and nonpassing. A run SHALL succeed only when every requested capture is complete, repeatable, compatible, and exactly matches its approved baseline. Machine-readable results SHALL identify mismatches, nondeterminism, missing approval, incompatibility, and execution errors distinctly.

#### Scenario: No approved baseline exists
- **WHEN** valid candidate captures have no approved baseline
- **THEN** results preserve those captures, report approval-required, and return nonzero without creating a baseline

#### Scenario: One selected capture differs
- **WHEN** all selected captures complete but one differs from its compatible approved baseline
- **THEN** the run returns nonzero and identifies that capture as a mismatch

#### Scenario: Every selected capture matches
- **WHEN** every selected capture is complete, repeatable, compatible, and exactly matches its approved baseline
- **THEN** the fidelity run succeeds

### Requirement: Keep visual reporting separate from verdicts
When requested, the suite SHALL produce a local HTML report showing reference, current, and difference images with changed-pixel counts and bounds where available. Reporting options and diagnostics SHALL NOT alter the automated image verdict.

#### Scenario: Report enabled or disabled
- **WHEN** the same run is performed with or without the optional report
- **THEN** its machine-readable results and pass or fail verdict are unchanged

### Requirement: Preserve evidence and continue independent cases
Capture failures and timeouts SHALL retain available evidence, allow independent selected cases to continue, and cause the overall run to return a nonzero result.

#### Scenario: A case fails or times out
- **WHEN** capture for a case fails or times out
- **THEN** available evidence is retained, independent cases continue, and the overall run is nonpassing
