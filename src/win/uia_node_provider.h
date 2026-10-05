#pragma once

#include "uia_constants.h"
#include "broa11y/node.h"

#include <string>
#include <string_view>

namespace broa11y::uia {

class UiaNodeProvider {
public:
    explicit UiaNodeProvider(Node* node);

    [[nodiscard]] NodeId id() const noexcept;
    [[nodiscard]] Node* node() const noexcept { return node_; }

    [[nodiscard]] std::string get_property_value(int32_t property_id) const;
    [[nodiscard]] bool is_pattern_supported(int32_t pattern_id) const;

    [[nodiscard]] NodeId navigate(NavigateDirection direction) const;

    bool execute_action(int32_t pattern_id, std::string_view action_name);

private:
    Node* node_ = nullptr;
};

} // namespace broa11y::uia
