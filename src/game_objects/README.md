# Game objects

`Character` and `PlayerCharacter` currently provide a small object-oriented
bridge for the Krisp playable-character demo. They own no global gameplay
state beyond an object's skeleton and active looping animation.

## TODO: move character behaviour to ECS

As NPCs and more character types are added, replace the façade classes with
composable ECS components and systems:

- `LocomotionComponent`: desired movement, speed, grounded state.
- `AnimationControllerComponent`: active locomotion clip and transitions.
- `CharacterControllerComponent`: capsule dimensions and collision settings.
- `FootIkComponent`: leg chains and terrain-query settings.
- `PlayerInputComponent` / `NpcAiComponent`: write desired movement for their
  respective control sources.

The player input and NPC AI systems should both feed the same locomotion,
animation, collision, and IK systems. Keep `Character` only as a convenience
factory or remove it once entity creation is component-based.
