#pragma once

#include "broa11y/bridge.h"
#include "broa11y/types.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace broa11y {

struct LinuxBridgeConfig {
    std::string app_name = "bro";
    std::string toolkit_name = "broa11y";
    // Where the window's top-left corner is on screen, for clients asking in
    // screen coordinates. Node bounds are window-relative; without this (or on
    // Wayland, which has no global coordinates) screen equals window.
    std::function<PointF()> window_origin;
};

// AT-SPI 2 provider. initialize() finds the accessibility bus (the
// AT_SPI_BUS_ADDRESS variable, else org.a11y.Bus on the session bus), exports
// every node under /org/a11y/atspi/accessible and embeds the tree's root in
// the registry, which is what makes the application appear to Orca and every
// other libatspi client. Requests are answered only inside process_events(),
// on the caller's thread: call it from the UI loop, ideally when poll_fd()
// becomes readable.
class LinuxBridge : public Bridge {
public:
    explicit LinuxBridge(LinuxBridgeConfig config = {});
    ~LinuxBridge() override;

    LinuxBridge(const LinuxBridge&) = delete;
    LinuxBridge& operator=(const LinuxBridge&) = delete;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "AT-SPI 2 (Linux)"; }
    [[nodiscard]] bool is_active() const noexcept override;
    [[nodiscard]] std::string last_error() const override;

    // The accessibility-bus connection's file descriptor, for poll(); -1 when
    // inactive. Wait for the events in poll_events() and the timeout in
    // poll_timeout_ms() (-1 = none).
    [[nodiscard]] int poll_fd() const;
    [[nodiscard]] short poll_events() const;
    [[nodiscard]] int poll_timeout_ms() const;
    // The connection's unique bus name, e.g. ":1.42"; empty when inactive.
    [[nodiscard]] std::string bus_name() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
