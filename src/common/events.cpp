#include "broa11y/events.h"

namespace broa11y {

std::string_view event_type_to_string(EventType type) {
    switch (type) {
        case EventType::NodeAdded: return "node_added";
        case EventType::NodeRemoved: return "node_removed";
        case EventType::BoundsChanged: return "bounds_changed";
        case EventType::PropertyChanged: return "property_changed";
        case EventType::FocusChanged: return "focus_changed";
        case EventType::CaretMoved: return "caret_moved";
        case EventType::TextSelectionChanged: return "text_selection_changed";
        case EventType::Announcement: return "announcement";
        case EventType::StateChanged: return "state_changed";
        case EventType::ValueChanged: return "value_changed";
        case EventType::ChildrenChanged: return "children_changed";
        case EventType::WindowActivated: return "window_activated";
        case EventType::WindowDeactivated: return "window_deactivated";
        case EventType::Count: return "unknown";
    }
    return "unknown";
}

} // namespace broa11y
