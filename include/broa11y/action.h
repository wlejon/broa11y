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
// Requests from assistive technology that the application owns the outcome
// of; the bridges send them to the node's ActionHandler, never change the
// model themselves:
//   set_value      string_val (text nodes) or number_val (nodes with a value)
//   set_selection  range_val, UTF-8 byte offsets; empty = move the caret there
//   focus          move keyboard focus to the node
inline constexpr std::string_view kActionSetSelection = "set_selection";

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
    TextRange range_val{};
};

using ActionHandler = std::function<bool(NodeId node_id, std::string_view action_name, const ActionParams& params)>;

} // namespace broa11y
