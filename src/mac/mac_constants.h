#pragma once

#include <string_view>

namespace broa11y::mac {

// Attributes
inline constexpr std::string_view kRoleAttribute = "AXRole";
inline constexpr std::string_view kSubroleAttribute = "AXSubrole";
inline constexpr std::string_view kRoleDescriptionAttribute = "AXRoleDescription";
inline constexpr std::string_view kTitleAttribute = "AXTitle";
inline constexpr std::string_view kDescriptionAttribute = "AXDescription";
inline constexpr std::string_view kValueAttribute = "AXValue";
inline constexpr std::string_view kParentAttribute = "AXParent";
inline constexpr std::string_view kChildrenAttribute = "AXChildren";
inline constexpr std::string_view kPositionAttribute = "AXPosition";
inline constexpr std::string_view kSizeAttribute = "AXSize";
inline constexpr std::string_view kEnabledAttribute = "AXEnabled";
inline constexpr std::string_view kFocusedAttribute = "AXFocused";

// Parameterized Attributes
inline constexpr std::string_view kLineForIndexAttribute = "AXLineForIndex";
inline constexpr std::string_view kRangeForLineAttribute = "AXRangeForLine";
inline constexpr std::string_view kStringForRangeAttribute = "AXStringForRange";

// Actions
inline constexpr std::string_view kPressAction = "AXPress";
inline constexpr std::string_view kIncrementAction = "AXIncrement";
inline constexpr std::string_view kDecrementAction = "AXDecrement";
inline constexpr std::string_view kShowMenuAction = "AXShowMenu";

// Notifications
inline constexpr std::string_view kFocusedUIElementChangedNotification = "AXFocusedUIElementChanged";
inline constexpr std::string_view kValueChangedNotification = "AXValueChanged";
inline constexpr std::string_view kTitleChangedNotification = "AXTitleChanged";
inline constexpr std::string_view kUIElementDestroyedNotification = "AXUIElementDestroyed";
inline constexpr std::string_view kSelectedChildrenChangedNotification = "AXSelectedChildrenChanged";
inline constexpr std::string_view kSelectedTextChangedNotification = "AXSelectedTextChanged";
inline constexpr std::string_view kAnnouncementRequestedNotification = "AXAnnouncementRequested";

} // namespace broa11y::mac
