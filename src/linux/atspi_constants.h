#pragma once

#include <cstdint>
#include <string_view>

namespace broa11y::atspi {

inline constexpr std::string_view kDbusNameRegistry = "org.a11y.atspi.Registry";
inline constexpr std::string_view kDbusInterfaceAccessible = "org.a11y.atspi.Accessible";
inline constexpr std::string_view kDbusInterfaceComponent = "org.a11y.atspi.Component";
inline constexpr std::string_view kDbusInterfaceAction = "org.a11y.atspi.Action";
inline constexpr std::string_view kDbusInterfaceText = "org.a11y.atspi.Text";
inline constexpr std::string_view kDbusInterfaceEditableText = "org.a11y.atspi.EditableText";
inline constexpr std::string_view kDbusInterfaceValue = "org.a11y.atspi.Value";

inline constexpr std::string_view kDbusInterfaceEventObject = "org.a11y.atspi.Event.Object";
inline constexpr std::string_view kDbusInterfaceEventWindow = "org.a11y.atspi.Event.Window";

inline constexpr std::string_view kPathRoot = "/org/a11y/atspi/accessible/root";
inline constexpr std::string_view kPathPrefix = "/org/a11y/atspi/accessible/";

enum class CoordType : uint32_t {
    Screen = 0,
    Window = 1,
    Parent = 2
};

enum class TextBoundaryType : uint32_t {
    Char = 0,
    WordStart = 1,
    WordEnd = 2,
    SentenceStart = 3,
    SentenceEnd = 4,
    LineStart = 5,
    LineEnd = 6
};

} // namespace broa11y::atspi
