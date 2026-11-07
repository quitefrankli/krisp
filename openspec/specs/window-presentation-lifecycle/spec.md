# window-presentation-lifecycle Specification

## Purpose

Keep rendering, camera sizing and capture coherent across native window size changes, minimisation and restoration without imposing consumer simulation pause policy.

## Requirements

### Requirement: Resize native presentation
The engine SHALL support resizing and maximising its native window, recover from recoverable surface changes, and render at the current nonzero framebuffer extent without restarting or losing scene state.

#### Scenario: Resize repeatedly
- **WHEN** the window changes dimensions repeatedly or is maximised and restored
- **THEN** rendering resumes at the latest supported framebuffer extent with scene state preserved
- **AND** obsolete presentation resources do not accumulate without bound

#### Scenario: Surface becomes out of date
- **WHEN** image acquisition or presentation reports a recoverable surface configuration change
- **THEN** the engine recreates presentation resources without treating the change as a fatal engine error
- **AND** unsuccessful attempts are not reported as successful presentations

### Requirement: Suspend minimised rendering
The engine SHALL suspend image acquisition, drawing and presentation while minimised or while either framebuffer dimension is zero, without busy-spinning. Simulation, audio and native event processing SHALL continue.

#### Scenario: Minimise and restore
- **WHEN** a running window is minimised and later restored to a nonzero framebuffer size
- **THEN** rendering suspends and subsequently resumes with the latest scene state and correct dimensions
- **AND** simulation and audio continue during the suspension

#### Scenario: Close or finish loading while minimised
- **WHEN** the window is minimised during loading or receives a close request while rendering is suspended
- **THEN** event processing remains responsive and shutdown completes without waiting indefinitely for a new presentation
- **AND** readiness is not falsely reported before its required presentation occurs

### Requirement: Keep camera and input sizing coherent
The engine SHALL update perspective and orthographic camera projections for the framebuffer aspect ratio, preserve zoom and orientation, and keep cursor picking aligned with rendered content. Logical window size and framebuffer pixel size SHALL remain distinct.

#### Scenario: Aspect ratio and display scale change
- **WHEN** the framebuffer aspect ratio or display scale changes
- **THEN** scene proportions and cursor picking match the current viewport without zero-dimension division
- **AND** camera zoom and orientation remain unchanged

### Requirement: Preserve UI and capture integrity
The engine SHALL refresh UI image bindings after presentation resource replacement. Recording SHALL stop cleanly when framebuffer dimensions change, preserving completed output without terminating rendering. Captures SHALL complete with their recorded dimensions or report failure explicitly rather than hang or return stale image data.

#### Scenario: Resize during recording
- **WHEN** an active recording encounters changed framebuffer dimensions
- **THEN** completed output is finalised at its original dimensions and recording becomes inactive
- **AND** rendering continues at the new dimensions

#### Scenario: Suspend or recreate with pending capture
- **WHEN** presentation is suspended or recreated with outstanding capture requests
- **THEN** submitted captures are completed safely and unsubmitted requests that cannot progress fail explicitly
- **AND** capture waiters are released on shutdown

### Requirement: Presentation completion support
The engine SHALL require presentation completion fence support for safe swapchain resource cleanup and SHALL report unsupported devices explicitly at startup.

#### Scenario: Unsupported graphics device
- **WHEN** no available device supports the required presentation completion mechanism and surface dependencies
- **THEN** startup fails with an actionable requirement diagnostic rather than using unsafe cleanup
