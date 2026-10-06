#pragma once

#include "broa11y/bridge.h"

#include <memory>
#include <string>
#include <string_view>

namespace broa11y {

struct WinBridgeConfig {
    std::string app_name = "bro";
    // The HWND whose client area the tree describes. Required: UI Automation
    // finds providers through the window's WM_GETOBJECT, and node bounds are
    // read as client-area pixels of this window.
    void* hwnd = nullptr;
};

// UI Automation provider. initialize() must run on the thread that owns the
// window, with that thread either uninitialized for COM or in a
// single-threaded apartment; every provider call then arrives on that thread
// through its message loop, so pumping messages is what answers clients.
// process_events() pumps nothing itself and only exists for the interface.
class WinBridge : public Bridge {
public:
    explicit WinBridge(WinBridgeConfig config = {});
    ~WinBridge() override;

    WinBridge(const WinBridge&) = delete;
    WinBridge& operator=(const WinBridge&) = delete;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "UI Automation (Windows)"; }
    [[nodiscard]] bool is_active() const noexcept override;
    [[nodiscard]] std::string last_error() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
