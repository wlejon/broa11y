# broa11y

[![CI](https://github.com/wlejon/broa11y/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/broa11y/actions/workflows/ci.yml)

Accessibility for applications built on the [bro](https://github.com/wlejon/bro)
runtime: an accessibility tree the application keeps current, and a bridge per
platform that serves it to screen readers: AT-SPI 2 on Linux (Orca), UI
Automation on Windows (Narrator, NVDA, JAWS), NSAccessibility on macOS
(VoiceOver). Terminal emulators get a text model of their grid. A standalone
C++20 library: no dependency on bro or bronze, no JS binding, its own CMake
and ctest.

## Model

The application owns a `Tree` of `Node`s: role, name, description, states,
bounds, an optional value range, text with caret and selection, attributes,
relations and named actions. Every mutation emits an `Event` (property, state,
bounds, value, caret, selection, children, focus, announcement); a
`TreeTransaction` batches a set of changes into one burst of events, or rolls
them back (removed nodes, handlers included, come back).

The bridges never change the model themselves. A screen reader's request (press
a button, set a value, move the caret, take focus) reaches the node's
`ActionHandler` as an action; the application does what it means and updates
the tree, and the tree's events become the platform's notifications.

```cpp
broa11y::Tree tree;
auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
window->set_name("Editor");
window->set_bounds({0, 0, 800, 600});

auto* run = tree.create_node_with_role(broa11y::Role::Button, 2);
tree.reparent_node(2, 1);
run->set_name("Run");
run->set_bounds({10, 10, 80, 28});
run->add_action({.name = std::string(broa11y::kActionActivate), .description = "Runs", .key_binding = "F5"});
run->set_action_handler([&](broa11y::NodeId, std::string_view action, const broa11y::ActionParams&) {
    if (action != broa11y::kActionActivate) return false;
    start_run();                                  // the application's own code
    tree.announce("Run started");
    return true;
});

broa11y::LinuxBridge bridge({.app_name = "editor"});   // WinBridge / MacBridge elsewhere
if (!bridge.initialize(&tree)) log("accessibility off: " + bridge.last_error());
// UI loop: poll bridge.poll_fd(), then bridge.process_events()
```

Text is UTF-8 and the model's caret, selection and terminal offsets are UTF-8
byte offsets. Each bridge converts: AT-SPI counts characters, UI Automation and
NSAccessibility count UTF-16 units. Character, word, line and paragraph
boundaries are computed once (`src/common/text_util`) and shared, so every
platform reports the same units. `TerminalAccessibility` turns a grid of rows
(soft-wrapped or not) into one text node with cursor and selection; columns
count characters.

```
include/broa11y/
  types.h, role.h, state.h, action.h, events.h   the vocabulary
  node.h, tree.h        Node, Tree, TreeTransaction
  terminal.h            TerminalAccessibility (grid -> text node)
  bridge.h              Bridge: initialize / shutdown / process_events / last_error
  linux_bridge.h, win_bridge.h, mac_bridge.h   one per platform
  broa11y.h             umbrella header (includes this platform's bridge)
```

## Platforms

Each bridge compiles only on its own platform. When the service it needs is
not there, `initialize()` returns false and `last_error()` says why.

| | Linux | Windows | macOS |
|---|---|---|---|
| API | AT-SPI 2 over D-Bus (sd-bus, libsystemd) | UI Automation server-side providers | NSAccessibility (`NSAccessibilityElement`) |
| Needs | the accessibility bus: `AT_SPI_BUS_ADDRESS`, or `org.a11y.Bus` on the session bus (at-spi2-core) | `WinBridgeConfig::hwnd`; `initialize()` on the window's thread, COM single-threaded apartment | `MacBridgeConfig::ns_view`; main thread; the view forwards `accessibilityChildren`, `accessibilityHitTest:` and `accessibilityFocusedUIElement` to the bridge (see `mac_bridge.h`) |
| Where requests are answered | `process_events()`, on the caller's thread (`poll_fd()` for the loop) | the window's message loop | the main run loop |
| Exposes | Accessible, Application, Component, Action, Text, EditableText, Value, Cache; registers with the registry by `Socket.Embed` | Invoke, Toggle, Value, RangeValue, Text (with text ranges) patterns; fragment navigation, hit testing, focus | roles, labels, values, frames, children, press / increment / decrement, focus, text ranges, line queries |
| Events | `org.a11y.atspi.Event.Object` (PropertyChange, StateChanged, ChildrenChanged, TextChanged, TextCaretMoved, TextSelectionChanged, BoundsChanged, Announcement), `Event.Window` | property-changed, focus, structure-changed, text, invoke, notification (`UiaRaiseNotificationEvent`) | `NSAccessibilityPostNotification` (focus, value, title, selected text, layout, created / destroyed, announcement) |

Node bounds are window-relative (client-area pixels on Windows, view points
with a top-left origin on macOS); Linux takes the window's screen origin from
`LinuxBridgeConfig::window_origin` when the platform has global coordinates.

Not modelled: per-glyph geometry (a text range's rectangle is its node's, and a
point inside text maps to no offset), text attributes, sentence boundaries
(answered as lines), more than one selection, and clipboard operations
(EditableText Copy/Cut/Paste are refused; the application owns its clipboard).

## Building

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release        # Windows: cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requirements: CMake 3.24+, a C++20 compiler (MSVC 2022, GCC 12+, Clang 15+,
Apple Clang), on Linux `libsystemd` (sd-bus, >= 246) with pkg-config. The
Linux bridge test also uses `dbus-daemon`, at-spi2-core's
`at-spi-bus-launcher` and registry, and libatspi's development files
(`atspi-2`); without them it skips and says which is missing. Windows links
`UIAutomationCore`, macOS `AppKit`. There are no sibling repos to fetch.

Add it to another CMake project with `add_subdirectory(broa11y)` and link
`broa11y::broa11y`.

## Tests

Real ctests: no `assert()`, failures count in every configuration, exit 77 is
a skip with the reason printed.

| Test | What it checks against |
|---|---|
| test_tree, test_transaction, test_actions | the model: navigation, hit testing, focus, cycle-free reparenting, commit / rollback (a removed subtree comes back), actions only through handlers |
| test_text_index | UTF-8 / character / UTF-16 conversions, invalid input, unit boundaries |
| test_terminal_a11y | the grid as text, cursor and selection mapping, non-ASCII rows |
| test_win_uia (Windows) | a hidden window serves the tree; a second process using the UI Automation client API (`IUIAutomation`) reads it, drives Invoke / Toggle / Value / RangeValue / Text / focus, and waits for the property, structure and notification events |
| test_linux_atspi (Linux) | a private session bus with the real `at-spi-bus-launcher` and registry; a second process built on libatspi (Orca's library) finds the application through the desktop, reads the tree, drives actions, values, text, selection and focus, and waits for the events |
| test_mac_bridge (macOS) | a real `NSWindow` and view; AppKit's window traversal must reach the tree, then the elements are read and driven through the NSAccessibility protocol. A cross-process `AXUIElement` client needs the Accessibility permission CI does not grant, so posted notifications are not observed |

## License

MIT, see [LICENSE](LICENSE).
