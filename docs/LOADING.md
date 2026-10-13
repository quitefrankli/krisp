# Responsive startup loading

`GameEngine::run()` accepts an optional scene-initialization callback. Construct
the engine first, then provide startup scene work to `run()`:

```cpp
engine.run([](GameEngine& engine)
{
	const auto assets = engine.assets().load_asset("content/scene.kasset");
	const auto assembly = AssetRegistry::default_assembly(assets);
	const auto model = engine.assets().instantiate_model(engine.get_ecs(), assembly);
	// Create scene objects and attach the model's renderables here.
});
```

The GLFW window is created hidden. Vulkan-backed engine construction still
runs synchronously before `run()`, so the desktop-managed window first appears
when `run()` begins and shows it. The graphics loop displays a centered
`Loading...` overlay while startup proceeds. The initial loading frame is
presented before the scene-initialization callback runs.

During this phase, registered asset sources are prepared on a CPU worker. This
work includes file or package reads, document parsing, image decode, geometry
decode, and derived mesh data. The worker populates source-owned CPU caches.
It does not mutate the live ECS or the global resource-provenance registry.
Asset adoption into live mesh and material stores, skeleton creation,
provenance registration, object creation, and renderable attachment remain on
the game thread. Model adoption provides a window-event checkpoint between
mesh-node groups.

The game thread continues polling GLFW events while AssetRegistry CPU work runs,
checking the worker every 10 ms. The same event-pumping path runs at model
adoption checkpoints and while waiting for graphics startup. Keep other CPU
work in the scene-initialization callback and `IApplication::on_begin()` short:
those callbacks run on the game thread and do not pump events while arbitrary
consumer code is executing. Closing the window or pressing Escape requests
shutdown. If CPU preparation is already in flight, `run()` continues servicing
window events and waits for that task to finish before releasing its captured
state and shutting down.

Loading suppresses gameplay and editor input. Escape remains available to
request shutdown. Window-event callbacks and graphics-thread ImGui drawing
share the engine UI state mutex; it is recursive because a GLFW callback may
re-enter an engine UI method while event polling holds the lock.

Once the callback and `IApplication::on_begin()` finish, the game thread
publishes the completed startup scene. The graphics thread prepares its
renderables incrementally, with a limit of eight renderables or four
milliseconds of cooperative work per draw iteration. Environment-lighting
setup and skeleton-buffer preparation happen before that budgeted loop. A
single Vulkan upload cannot be interrupted, so one large renderable may exceed
the time target. This cooperative budget does not provide a hard real-time
guarantee. The loading overlay remains visible until the graphics thread
acknowledges that the completed startup frame has been presented; gameplay
ticks begin after that acknowledgement.

CPU preparation and GPU realization have separate owners. CPU workers may
populate registered-source caches; live ECS adoption belongs to the game
thread; scene Vulkan resource creation, upload, and rendering belong to the
graphics thread. `AssetRegistry`'s temporary loading callbacks are installed
only for startup and removed before gameplay begins. This flow does not promise
an asynchronous gameplay asset-loading API; asset operations outside startup
use their existing synchronous behavior.

Mesh and material constructors may run on CPU workers while rendering is
active. `GenericID` therefore allocates IDs with an atomic counter. Restoring
serialized IDs uses `raise_next_id()` to advance a counter monotonically without
overwriting a concurrently allocated ID. `set_next_id()` remains available for
explicit counter initialization and tests; callers should not use it to restore
IDs while allocations are active.
