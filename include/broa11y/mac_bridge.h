#pragma once

#include "broa11y/bridge.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

struct MacBridgeConfig {
    std::string app_name = "bro";
    bool mock_mode = true;
};

class MacBridge : public Bridge {
public:
    explicit MacBridge(MacBridgeConfig config = {});
    ~MacBridge() override;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "NSAccessibility (macOS)"; }
    [[nodiscard]] bool is_active() const noexcept override;

    // Test / Inspection hooks
    [[nodiscard]] size_t emitted_notification_count() const noexcept;
    [[nodiscard]] std::vector<std::string> get_emitted_notifications() const;
    void clear_emitted_notifications();

    // Query NSAccessibility attribute on an element
    [[nodiscard]] std::string query_element_attribute(NodeId node_id, std::string_view attribute) const;
    [[nodiscard]] std::string query_parameterized_attribute(NodeId node_id,
                                                           std::string_view attribute,
                                                           std::string_view parameter) const;
    bool perform_element_action(NodeId node_id, std::string_view action);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
