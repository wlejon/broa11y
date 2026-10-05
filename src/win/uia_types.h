#pragma once

#include "broa11y/types.h"

#include <cstdint>
#include <string>

namespace broa11y::uia {

struct UiaEvent {
    int32_t event_id = 0;
    NodeId node_id = kInvalidNodeId;
    int32_t property_id = 0;
    std::string old_value{};
    std::string new_value{};
    std::string description{};

    bool operator==(const UiaEvent& other) const = default;
};

} // namespace broa11y::uia
