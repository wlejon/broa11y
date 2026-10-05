#pragma once

#include "broa11y/types.h"

#include <string>
#include <unordered_map>

namespace broa11y::mac {

struct MacNotification {
    std::string name;
    NodeId node_id = kInvalidNodeId;
    std::unordered_map<std::string, std::string> user_info;

    bool operator==(const MacNotification& other) const = default;
};

} // namespace broa11y::mac
