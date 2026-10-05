#pragma once

#include "broa11y/types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

inline constexpr std::string_view kActionActivate = "activate";
inline constexpr std::string_view kActionFocus = "focus";
inline constexpr std::string_view kActionSetValue = "set_value";
inline constexpr std::string_view kActionScroll = "scroll";
inline constexpr std::string_view kActionShowMenu = "show_menu";
inline constexpr std::string_view kActionDismiss = "dismiss";
inline constexpr std::string_view kActionSelect = "select";

struct ActionDescriptor {
    std::string name;
    std::string description;
    std::string key_binding;

    bool operator==(const ActionDescriptor& other) const = default;
};

struct ActionParams {
    std::string string_val;
    double number_val = 0.0;
    int32_t int_val = 0;
    PointF point_val{};
};

using ActionHandler = std::function<bool(NodeId node_id, std::string_view action_name, const ActionParams& params)>;

} // namespace broa11y
