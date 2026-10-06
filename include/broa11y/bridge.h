#pragma once

#include "broa11y/events.h"

#include <string>
#include <string_view>

namespace broa11y {

class Tree;

// Exposes a Tree to the platform's assistive technology. One implementation
// per platform, each compiled only there: LinuxBridge (AT-SPI 2 over D-Bus),
// WinBridge (UI Automation providers), MacBridge (NSAccessibility).
//
// initialize() connects the tree and starts listening to its events; when the
// platform service is not there (no accessibility bus, no window to host the
// providers) it returns false and last_error() says why. Every query from an
// assistive technology is answered on the thread that calls process_events()
// (Linux) or that owns the host window / view (Windows, macOS), so the tree is
// only ever touched from the application's own UI thread.
class Bridge {
public:
    virtual ~Bridge() = default;

    virtual bool initialize(Tree* tree) = 0;
    virtual void shutdown() = 0;
    // Called for every tree event once initialize() has succeeded; public so an
    // application can forward events it synthesises itself.
    virtual void handle_event(const Event& event) = 0;
    // Answers pending requests from assistive technologies without blocking.
    virtual void process_events() = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool is_active() const noexcept = 0;
    // Why initialize() failed, or the last error since; empty when none.
    [[nodiscard]] virtual std::string last_error() const = 0;
};

} // namespace broa11y
