# broa11y

`broa11y` is a standalone, lightweight modern C++20 accessibility library with an accessibility tree model and platform bridges (AT-SPI 2 on Linux, UI Automation on Windows, NSAccessibility on macOS), designed to make GUI toolkits, widgets, and terminal emulators (`broterm` / `bropty`) accessible to screen readers and assistive technologies.

---

## Features

1. **Core Accessibility Tree Model**
   - **Hierarchical Node Tree**: Strongly typed `NodeId` identifiers, parent/children pointers, tree navigation (`parent`, `first_child`, `last_child`, `next_sibling`, `previous_sibling`, `child_at`, `index_in_parent`).
   - **Comprehensive Roles**: Standard `Role` enum including `Application`, `Window`, `Dialog`, `Alert`, `Button`, `CheckBox`, `RadioButton`, `TextInput`, `Terminal`, `Label`, `Link`, `List`, `ListItem`, `Menu`, `MenuItem`, `MenuBar`, `Slider`, `ProgressBar`, `ScrollBar`, `Tree`, `Table`, `Panel`, `Tab`, `TabList`, and more.
   - **Bitset States**: `StateSet` bitset managing accessibility states (`Focused`, `Focusable`, `Selected`, `Selectable`, `Expanded`, `Collapsed`, `Disabled`, `ReadOnly`, `Checked`, `Busy`, `Modal`, `MultiSelectable`, `Visible`, `Showing`, `Sensitive`, `MultiLine`, etc.).
   - **Properties & Attributes**: Accessible names, descriptions, bounding rectangles (`RectF`), numeric value ranges (`ValueRange`), text content, caret position, and selection range (`TextRange`).
   - **Actions**: Standard action descriptors (`activate`, `focus`, `set_value`, `scroll`, `show_menu`, `dismiss`) and custom actions with flexible `ActionHandler` callbacks.
   - **Events & Mutations**: Dispatches `NodeAdded`, `NodeRemoved`, `BoundsChanged`, `PropertyChanged`, `FocusChanged`, `CaretMoved`, `TextSelectionChanged`, `StateChanged`, `ValueChanged`, `ChildrenChanged`, `Announcement`.
   - **Transaction & Mutation Batching**: RAII transaction model (`TreeTransaction`) supporting atomic commit and rollback with event batching.

2. **Terminal Accessibility**
   - Dedicated support for `Role::Terminal` tailored for terminal emulators (e.g. `bropty` / `broterm`).
   - Maps 2D character grids and lines into accessible linear text, supporting soft line wraps vs hard newlines.
   - Multi-granularity navigation: `Character`, `Word`, `Line`, `Paragraph`, `Document` boundaries.
   - Cursor tracking: maps `(row, col)` coordinates to linear text caret offsets with automatic `CaretMoved` event emission.
   - Selection tracking: maps terminal visual selections to accessible `TextRange` selections.
   - Screen reader announcements: terminal bell (`\a`) announcements, command status notices, and live region speech prompts.

3. **Platform Bridges**
   - **Linux Bridge (`LinuxBridge`)**:
     - AT-SPI 2 over D-Bus (`org.a11y.Bus` / `org.a11y.atspi`).
     - Exposes AT-SPI interfaces: `Accessible`, `Component`, `Action`, `Text`, `EditableText`, `Value`.
     - Emits standard AT-SPI signals: `StateChanged`, `PropertyChange:AccessibleName`, `PropertyChange:AccessibleDescription`, `TextCaretMoved`, `TextSelectionChanged`, `ChildrenChanged`, `Window:Activate`, `Announcement`.
     - Runs with live `libdbus-1` connections or deterministic mock dispatch for headless testing.
   - **Windows Bridge (`WinBridge`)**:
     - Microsoft UI Automation (UIA) bridge with provider architecture (`UiaNodeProvider`).
     - Implements standard control patterns: `Invoke`, `Toggle`, `Value`, `RangeValue`, `Text`, `Selection`.
     - Translates tree mutations into UIA automation property and structure change events.
   - **macOS Bridge (`MacBridge`)**:
     - Apple NSAccessibility protocol bridge (`MacAccessibleElement`).
     - Implements accessibility attributes (`AXRole`, `AXTitle`, `AXValue`, `AXPosition`, `AXSize`, etc.), parameterized attributes (`AXStringForRange`), and actions (`AXPress`, `AXIncrement`).
     - Emits standard NSAccessibility notifications (`AXFocusedUIElementChanged`, `AXValueChanged`, `AXTitleChanged`, `AXAnnouncementRequested`).

---

## Directory Structure

```text
broa11y/
├── include/broa11y/          # Public headers
│   ├── types.h               # Core geometry, IDs, enums
│   ├── role.h                # Accessible roles & mappings
│   ├── state.h               # StateSet bitset & state enums
│   ├── action.h              # Action descriptors & handlers
│   ├── events.h              # Tree events & payloads
│   ├── node.h                # Node & NodeData representation
│   ├── tree.h                # Tree model & TreeTransaction
│   ├── terminal.h            # TerminalAccessibility model
│   ├── bridge.h              # Platform bridge interface & factory
│   ├── linux_bridge.h        # AT-SPI 2 Linux bridge
│   ├── win_bridge.h          # UI Automation Windows bridge
│   ├── mac_bridge.h          # NSAccessibility macOS bridge
│   ├── version.h             # Library version
│   └── broa11y.h             # Umbrella header
├── src/
│   ├── common/               # Core model implementation
│   ├── linux/                # AT-SPI 2 D-Bus wire protocol & adaptor
│   ├── win/                  # UI Automation providers & bridge
│   └── mac/                  # NSAccessibility elements & bridge
└── tests/                    # CTest test suite
```

---

## Building and Testing

`broa11y` uses CMake (>= 3.24) and Ninja.

### Build:
```bash
cmake -B build -G Ninja
cmake --build build -j 2
```

### Run Tests:
```bash
ctest --test-dir build --output-on-failure
```

---

## Quick Example

```cpp
#include <broa11y/broa11y.h>
#include <iostream>

int main() {
    broa11y::Tree tree;

    // 1. Create a root window
    auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
    window->set_name("My Terminal App");
    window->set_bounds({0, 0, 1024, 768});

    // 2. Attach terminal accessibility
    auto* term_node = tree.create_node(2);
    tree.reparent_node(2, 1);

    broa11y::TerminalAccessibility term;
    term.attach_to_node(&tree, 2);
    term.append_line("user@machine:~$ echo Hello", false);
    term.append_line("Hello", false);
    term.set_cursor(1, 5);

    // 3. Connect platform bridge
    auto bridge = broa11y::create_platform_bridge(&tree);

    // 4. Focus terminal
    tree.set_focus(2);

    // 5. Announce screen reader alert
    term.announce("Output received", broa11y::AnnouncementPriority::Polite);

    return 0;
}
```

---

## License

MIT License (see [LICENSE](LICENSE)).
