#pragma once

#include <cstdint>

namespace broa11y::uia {

// Pattern IDs
inline constexpr int32_t kInvokePatternId = 10000;
inline constexpr int32_t kSelectionPatternId = 10001;
inline constexpr int32_t kValuePatternId = 10002;
inline constexpr int32_t kRangeValuePatternId = 10003;
inline constexpr int32_t kScrollPatternId = 10004;
inline constexpr int32_t kExpandCollapsePatternId = 10005;
inline constexpr int32_t kTogglePatternId = 10015;
inline constexpr int32_t kTextPatternId = 10014;

// Property IDs
inline constexpr int32_t kBoundingRectanglePropertyId = 30001;
inline constexpr int32_t kControlTypePropertyId = 30003;
inline constexpr int32_t kNamePropertyId = 30005;
inline constexpr int32_t kHasKeyboardFocusPropertyId = 30008;
inline constexpr int32_t kIsKeyboardFocusablePropertyId = 30009;
inline constexpr int32_t kIsEnabledPropertyId = 30010;
inline constexpr int32_t kHelpTextPropertyId = 30013;
inline constexpr int32_t kValueValuePropertyId = 30045;
inline constexpr int32_t kRangeValueValuePropertyId = 30047;
inline constexpr int32_t kRangeValueMinimumPropertyId = 30049;
inline constexpr int32_t kRangeValueMaximumPropertyId = 30050;

// Event IDs
inline constexpr int32_t kStructureChangedEventId = 20002;
inline constexpr int32_t kAutomationPropertyChangedEventId = 20004;
inline constexpr int32_t kInvoke_InvokedEventId = 20009;
inline constexpr int32_t kText_TextSelectionChangedEventId = 20014;
inline constexpr int32_t kText_TextChangedEventId = 20015;
inline constexpr int32_t kNotificationEventId = 20035;

enum class NavigateDirection : int32_t {
    Parent = 0,
    NextSibling = 1,
    PreviousSibling = 2,
    FirstChild = 3,
    LastChild = 4
};

} // namespace broa11y::uia
