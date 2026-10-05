#pragma once

#include "broa11y/events.h"

#include <memory>
#include <string_view>

namespace broa11y {

class Tree;

class Bridge {
public:
    virtual ~Bridge() = default;

    virtual bool initialize(Tree* tree) = 0;
    virtual void shutdown() = 0;
    virtual void handle_event(const Event& event) = 0;
    virtual void process_events() = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual bool is_active() const noexcept = 0;
};

std::unique_ptr<Bridge> create_platform_bridge(Tree* tree);
std::unique_ptr<Bridge> create_linux_bridge(Tree* tree);
std::unique_ptr<Bridge> create_win_bridge(Tree* tree);
std::unique_ptr<Bridge> create_mac_bridge(Tree* tree);

} // namespace broa11y
