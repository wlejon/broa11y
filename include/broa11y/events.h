#pragma once

#include "broa11y/state.h"
#include "broa11y/types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace broa11y {

enum class EventType : uint32_t {
    NodeAdded = 0,
    NodeRemoved,
    BoundsChanged,
    PropertyChanged,
    FocusChanged,
    CaretMoved,
    TextSelectionChanged,
    Announcement,
    StateChanged,
    ValueChanged,
    ChildrenChanged,
    WindowActivated,
    WindowDeactivated,
    Count
};

std::string_view event_type_to_string(EventType type);

struct PropertyChangedPayload {
    std::string property_name;
    std::string old_value;
    std::string new_value;

    bool operator==(const PropertyChangedPayload&) const = default;
};

struct StateChangedPayload {
    State state = State::Focused;
    bool enabled = false;

    bool operator==(const StateChangedPayload&) const = default;
};

struct FocusChangedPayload {
    NodeId previous_focused_id = kInvalidNodeId;
    NodeId current_focused_id = kInvalidNodeId;

    bool operator==(const FocusChangedPayload&) const = default;
};

struct CaretMovedPayload {
    int32_t old_offset = 0;
    int32_t new_offset = 0;

    bool operator==(const CaretMovedPayload&) const = default;
};

struct TextSelectionPayload {
    TextRange selection{};

    bool operator==(const TextSelectionPayload&) const = default;
};

enum class ChildrenChangeType : uint8_t {
    ChildAdded = 0,
    ChildRemoved
};

struct ChildrenChangedPayload {
    ChildrenChangeType change_type = ChildrenChangeType::ChildAdded;
    NodeId child_id = kInvalidNodeId;
    size_t index = 0;

    bool operator==(const ChildrenChangedPayload&) const = default;
};

struct AnnouncementPayload {
    std::string message;
    AnnouncementPriority priority = AnnouncementPriority::Polite;

    bool operator==(const AnnouncementPayload&) const = default;
};

struct BoundsChangedPayload {
    RectF old_bounds{};
    RectF new_bounds{};

    bool operator==(const BoundsChangedPayload&) const = default;
};

struct ValueChangedPayload {
    ValueRange old_value{};
    ValueRange new_value{};

    bool operator==(const ValueChangedPayload&) const = default;
};

struct Event {
    EventType type = EventType::NodeAdded;
    NodeId node_id = kInvalidNodeId;

    std::variant<
        std::monostate,
        PropertyChangedPayload,
        StateChangedPayload,
        FocusChangedPayload,
        CaretMovedPayload,
        TextSelectionPayload,
        ChildrenChangedPayload,
        AnnouncementPayload,
        BoundsChangedPayload,
        ValueChangedPayload
    > payload;

    template <typename T>
    [[nodiscard]] const T* get_if() const {
        return std::get_if<T>(&payload);
    }
};

using EventListenerId = uint64_t;
using EventListener = std::function<void(const Event&)>;

} // namespace broa11y
