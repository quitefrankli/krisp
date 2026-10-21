## ADDED Requirements

### Requirement: Initial scene presentation timestamp
The engine SHALL expose an application-neutral monotonic timestamp for when it first observes presentation of the initial scene. The timestamp SHALL be set once before the engine reports readiness and remain available to its consumer.

#### Scenario: Initial scene is first presented
- **WHEN** the engine observes successful presentation containing its initial scene
- **THEN** it stores the monotonic nanosecond timestamp for that observation
- **AND** it does not replace the timestamp on later presentations

#### Scenario: Consumer queries before readiness
- **WHEN** a consumer queries the timestamp before the initial scene presentation is observed
- **THEN** the engine reports that no timestamp is available

#### Scenario: Consumer queries after readiness
- **WHEN** a consumer queries the timestamp after the initial scene presentation is observed
- **THEN** the engine returns the stored timestamp in the engine's monotonic clock domain
- **AND** the timestamp is independent of application-specific readiness policy
