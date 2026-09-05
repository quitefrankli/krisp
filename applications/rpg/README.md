# RPG application boundary

`rpg` is the home for RPG gameplay and presentation built on the Krisp engine.
It starts a playable `npc.glb` character with locomotion animations, a ground
plane, an equippable sword, and RPG status and equipment UI.

## Migration candidates

Move these first:

- `src/entity_component_system/equipment.*`: named wearable slots, grip
  transforms, and item ownership are game rules. Keep only generic skeletal
  attachment support in the engine.
- `src/game_objects/character.*` and `player_character.*`: animation choice,
  movement speed, ground snapping, and action playback define RPG character
  behaviour. The engine should expose input, physics, animation, and camera
  primitives rather than own a player class.
- The `PlayerCharacter` discovery and camera-follow policy in `GameEngine`:
  selecting the active player and deciding whether normal mode requires one
  should be application policy supplied through `IApplication`.

Review these after the first migration:

- Equipment serialization in the scene serializer should move with the
  equipment model once applications can extend scene persistence.
- `TileSystem` may belong here if it becomes RPG world or navigation logic. Its
  current spatial-grid operations are generic enough to remain in the engine.

Keep rendering, resource loading, animation and skeletal components, physics,
input, generic object/ECS lifetime, and the application UI registration API in
the engine.
