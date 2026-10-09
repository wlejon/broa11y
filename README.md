# broa11y

[![CI](https://github.com/wlejon/broa11y/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/broa11y/actions/workflows/ci.yml)

Accessibility for applications built on the [bro](https://github.com/wlejon/bro)
runtime: an accessibility tree the application keeps current, and a bridge per
platform that serves it to screen readers: AT-SPI 2 on Linux (Orca), UI
Automation on Windows (Narrator, NVDA, JAWS), NSAccessibility on macOS
(VoiceOver). Terminal emulators get a text model of their grid. A standalone
C++20 library: no dependency on bro or bronze, no JS binding in the core library,
its own CMake and ctest.

## Where it sits

Part of the **[bro](https://github.com/wlejon/bro)** desktop ecosystem (see the
[ecosystem architecture](https://github.com/wlejon/bro/blob/main/docs/ecosystem.md)).
Within the desktop stack, `broa11y` sits as the accessibility provider, bridging
bro's UI hierarchy and `<terminal>` elements to OS accessibility trees and
assistive technology screen readers across Windows, Linux, and macOS.

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
#include <broa11y/broa11y.h>

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

| Header | Contents |
| :--- | :--- |
| `types.h`, `role.h`, `state.h`, `action.h`, `events.h` | Model vocabulary, roles, states, actions, event types |
| `node.h`, `tree.h` | `Node`, `Tree`, `TreeTransaction` |
| `terminal.h` | `TerminalAccessibility` (grid -> text node mapping) |
| `bridge.h` | `Bridge`: base lifecycle interface (`initialize`, `shutdown`, `process_events`, `last_error`) |
| `linux_bridge.h`, `win_bridge.h`, `mac_bridge.h` | Platform-specific bridge implementations |
| `broa11y.h` | Master umbrella header (includes current platform bridge) |

## Platforms

Each bridge compiles only on its own platform. When the service it needs is
not available, `initialize()` returns false and `last_error()` explains why.

| | Linux | Windows | macOS |
|---|---|---|---|
| **API** | AT-SPI 2 over D-Bus (sd-bus, libsystemd) | UI Automation server-side providers | NSAccessibility (`NSAccessibilityElement`) |
| **Needs** | Accessibility bus: `AT_SPI_BUS_ADDRESS`, or `org.a11y.Bus` on the session bus (at-spi2-core) | `WinBridgeConfig::hwnd`; `initialize()` on the window's thread, COM single-threaded apartment (STA) | `MacBridgeConfig::ns_view`; main thread; the view forwards `accessibilityChildren`, `accessibilityHitTest:` and `accessibilityFocusedUIElement` to bridge |
| **Request handling** | `process_events()`, on caller's thread (`poll_fd()` for poll loop) | Window message loop | Main run loop |
| **Exposes** | Accessible, Application, Component, Action, Text, EditableText, Value, Cache; registers via `Socket.Embed` | Invoke, Toggle, Value, RangeValue, Text (with text ranges) patterns; fragment navigation, hit testing, focus | Roles, labels, values, frames, children, press / increment / decrement, focus, text ranges, line queries |
| **Events** | `org.a11y.atspi.Event.Object` (PropertyChange, StateChanged, ChildrenChanged, TextChanged, TextCaretMoved, TextSelectionChanged, BoundsChanged, Announcement), `Event.Window` | Property-changed, focus, structure-changed, text, invoke, notification (`UiaRaiseNotificationEvent`) | `NSAccessibilityPostNotification` (focus, value, title, selected text, layout, created / destroyed, announcement) |

Node bounds are window-relative (client-area pixels on Windows, view points
with a top-left origin on macOS); Linux takes the window's screen origin from
`LinuxBridgeConfig::window_origin` when the platform has global coordinates.

Not modelled: per-glyph geometry (a text range's rectangle is its node's, and a
point inside text maps to no offset), text attributes, sentence boundaries
(answered as lines), more than one selection, and clipboard operations
(EditableText Copy/Cut/Paste are refused; the application owns its clipboard).

## Building

### Prerequisites

- **CMake 3.24+** and a **C++20** compiler (MSVC 2022+, GCC 12+, Clang 15+, Apple Clang).
- **Linux**: `libsystemd` (sd-bus >= 246) via pkg-config (`libsystemd-dev` on Debian/Ubuntu, `systemd-libs` on Arch).
  The Linux bridge integration tests also use `dbus-daemon`, `at-spi-bus-launcher`, `at-spi2-registryd`, and `atspi-2` (`libatspi-dev`).
- **Windows**: `UIAutomationCore`, `ole32`, `oleaut32` (included with Windows SDK).
- **macOS**: `AppKit` (Objective-C++ ARC support).

### Standalone build

```bash
# Linux / macOS
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

# Windows (MSVC)
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake options:
- `BROA11Y_BUILD_TESTS`: Build tests (default `ON` when top-level, `OFF` when included via `add_subdirectory`).
- `BROA11Y_COVERAGE`: Instrument the build for gcov coverage (GCC/Clang).
- `BROA11Y_ENABLE_API`: Build the standalone Bronze JavaScript API (default `ON` when top-level). bronze (with brass) comes from `../bronze` beside the top-level project, else the head of its main branch, fetched at configure (`cmake/bro_deps.cmake`), so a plain `git clone` builds.

### Consuming broa11y

Downstream projects consume the `broa11y::broa11y` CMake target. Ecosystem
consumers declare it with `bro_dependency()` (`cmake/bro_deps.cmake`): a target the
outer project already added wins, else a `../broa11y` working tree beside the
top-level project, else the head of its main branch, fetched at configure
(`-DFETCHCONTENT_SOURCE_DIR_BROA11Y=<path>` points at another tree):

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)
bro_dependency(broa11y)

target_link_libraries(your_target PRIVATE broa11y::broa11y)
```

## Tests

Test assertions use `tests/check.h` (active in every configuration, no `assert()`).
A test that cannot run in the current environment exits code 77 with the reason
printed, and ctest reports it as skipped.

### Test matrix

| Test | Platform | Target / Environment | Oracle |
|---|---|---|---|
| `test_tree`, `test_transaction`, `test_actions` | everywhere | in-process | The model: navigation, hit testing, focus, cycle-free reparenting, commit / rollback (a removed subtree comes back), actions only through handlers |
| `test_text_index` | everywhere | in-process | UTF-8 / character / UTF-16 conversions, invalid input, unit boundaries |
| `test_terminal_a11y` | everywhere | in-process | Grid as text, cursor and selection mapping, non-ASCII rows |
| `test_win_uia` | Windows | Hidden HWND + separate UIA client process | Hidden window serves providers; a second process using `IUIAutomation` (as Narrator does) reads tree, drives Invoke / Toggle / Value / RangeValue / Text / focus, and asserts property, structure, and notification events |
| `test_linux_atspi` | Linux | Private D-Bus session bus + `at-spi-bus-launcher` + `at-spi2-registryd` | Private session bus with real AT-SPI registry; a second process built on `libatspi` (Orca's library) finds the application through the desktop root, reads the tree, drives actions, values, text, selection, and focus, and waits for events |
| `test_mac_bridge` | macOS | Real `NSWindow` + view | AppKit window traversal reaches tree; elements are read and driven through NSAccessibility protocol |
| `broa11y_test_api` | Linux / Windows (when API enabled) | Bronze runtime | Bronze JavaScript bindings (`broa11y_api`) and garbage collection stress testing |

### Test fixtures & CI skipping

- **Windows UI Automation**: Exercises the full provider implementation against the
  genuine Windows `IUIAutomation` COM client API in a helper process.
- **Linux AT-SPI 2 Registry**: Starts a private `dbus-daemon` session bus, launches
  `at-spi-bus-launcher`, and spawns `at-spi2-registryd` if present. If `atspi-2`,
  `dbus-daemon`, or `at-spi-bus-launcher` is missing, `test_linux_atspi` exits 77 (skipped)
  with the missing dependency printed.
- **macOS NSAccessibility**: Traverses the element hierarchy within the test process.
  Cross-process `AXUIElement` testing requires Accessibility permissions that macOS CI
  runners do not grant headless jobs, so posted notifications are verified via in-process
  inspection.

## License

MIT, see [LICENSE](LICENSE).
