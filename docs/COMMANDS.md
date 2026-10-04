# Slash commands

Press `/` outside another text field to open the command console in either
engine mode. It spans the viewport width and occupies its bottom half, with a
translucent background and no title bar, moving, or resizing. Command output
and matching suggestions appear above the input line at the bottom. Simulation
pauses while the console is open. Gameplay keyboard, mouse, and camera input
are captured; the cursor is released. Escape closes the console and restores
the previous cursor capture state. An existing explicit pause remains in
effect. Enter executes a command; feedback stays visible until the console is
closed. Tab completes the common prefix of matching command names (or the whole
name for a single match).

`/help` lists all registered commands. `/exit` requests orderly engine shutdown.
These commands accept no arguments.

Consumers register commands through `GameEngine::get_commands().add`, in
`IApplication::create_ui` or `on_begin`, before interactive use:

```cpp
engine.get_commands().add("inspect", "Inspect a resource",
    [](GameEngine& engine, std::string_view arguments) -> std::string {
        // Resolve arguments against consumer-owned resources.
        return "Inspection complete";
    });
```

Names omit the slash and contain lowercase ASCII letters, digits, underscores,
or hyphens. Empty names, empty handlers, and duplicate names throw
`std::invalid_argument`. Help and completion use the same registry, so custom
commands appear automatically. Handlers run on the game thread outside the UI
lock. Registration and direct execution also belong on that thread.

The handler receives the argument text after leading whitespace is removed.
Quoting, tokenization, validation, and argument-specific completion are not
provided; consumers interpret their argument text. The returned string is shown
as feedback. Standard exceptions are displayed as command failures.
