#pragma once

#include "broa11y/bridge.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

struct WinBridgeConfig {
    std::string app_name = "bro";
    bool mock_mode = true;
};

class WinBridge : public Bridge {
public:
    explicit WinBridge(WinBridgeConfig config = {});
    ~WinBridge() override;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "UI Automation (Windows)"; }
    [[nodiscard]] bool is_active() const noexcept override;

    // Test / Inspection hooks
    [[nodiscard]] size_t emitted_event_count() const noexcept;
    [[nodiscard]] std::vector<std::string> get_emitted_event_names() const;
    void clear_emitted_events();

    // Query UIA properties on a node provider
    [[nodiscard]] std::string query_provider_property(NodeId node_id, int32_t property_id) const;
    bool execute_provider_action(NodeId node_id, int32_t pattern_id, std::string_view action_name);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
