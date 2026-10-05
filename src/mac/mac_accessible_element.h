#pragma once

#include "mac_constants.h"
#include "broa11y/node.h"

#include <string>
#include <string_view>

namespace broa11y::mac {

class MacAccessibleElement {
public:
    explicit MacAccessibleElement(Node* node);

    [[nodiscard]] NodeId id() const noexcept;
    [[nodiscard]] Node* node() const noexcept { return node_; }

    [[nodiscard]] std::string get_attribute(std::string_view attribute) const;
    [[nodiscard]] std::string get_parameterized_attribute(std::string_view attribute,
                                                          std::string_view parameter) const;
    bool perform_action(std::string_view action);

private:
    Node* node_ = nullptr;
};

} // namespace broa11y::mac
