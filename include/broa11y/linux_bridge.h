#pragma once

#include "broa11y/bridge.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

struct LinuxBridgeConfig {
    std::string app_name = "bro";
    std::string toolkit_name = "broa11y";
};

class LinuxBridge : public Bridge {
public:
    explicit LinuxBridge(LinuxBridgeConfig config = {});
    ~LinuxBridge() override;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "AT-SPI 2 (Linux)"; }
    [[nodiscard]] bool is_active() const noexcept override;

    // Headless / Test inspection methods
    [[nodiscard]] size_t emitted_signal_count() const noexcept;
    [[nodiscard]] std::vector<std::string> get_emitted_signal_names() const;
    void clear_emitted_signals();

    // Direct invocation helper for testing method handlers without D-Bus socket
    std::string handle_method_call(std::string_view path,
                                  std::string_view interface_name,
                                  std::string_view method_name,
                                  const std::vector<std::string>& args = {});

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
