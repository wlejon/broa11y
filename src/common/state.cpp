#include "broa11y/state.h"

#include <sstream>

namespace broa11y {

std::string_view state_to_string(State state) {
    switch (state) {
        case State::Focused: return "focused";
        case State::Focusable: return "focusable";
        case State::Selected: return "selected";
        case State::Selectable: return "selectable";
        case State::Expanded: return "expanded";
        case State::Collapsed: return "collapsed";
        case State::Disabled: return "disabled";
        case State::ReadOnly: return "read_only";
        case State::Checked: return "checked";
        case State::Busy: return "busy";
        case State::Modal: return "modal";
        case State::MultiSelectable: return "multiselectable";
        case State::Visible: return "visible";
        case State::Showing: return "showing";
        case State::Sensitive: return "sensitive";
        case State::Defunct: return "defunct";
        case State::Active: return "active";
        case State::Armed: return "armed";
        case State::Indeterminate: return "indeterminate";
        case State::Vertical: return "vertical";
        case State::Horizontal: return "horizontal";
        case State::Required: return "required";
        case State::Invalid: return "invalid";
        case State::MultiLine: return "multi_line";
        case State::SingleLine: return "single_line";
        case State::HasPopup: return "has_popup";
        case State::SelectableText: return "selectable_text";
        case State::Editable: return "editable";
        case State::Animated: return "animated";
        case State::Transient: return "transient";
        case State::Count: return "unknown";
    }
    return "unknown";
}

State string_to_state(std::string_view str) {
    if (str == "focused") return State::Focused;
    if (str == "focusable") return State::Focusable;
    if (str == "selected") return State::Selected;
    if (str == "selectable") return State::Selectable;
    if (str == "expanded") return State::Expanded;
    if (str == "collapsed") return State::Collapsed;
    if (str == "disabled") return State::Disabled;
    if (str == "read_only") return State::ReadOnly;
    if (str == "checked") return State::Checked;
    if (str == "busy") return State::Busy;
    if (str == "modal") return State::Modal;
    if (str == "multiselectable") return State::MultiSelectable;
    if (str == "visible") return State::Visible;
    if (str == "showing") return State::Showing;
    if (str == "sensitive") return State::Sensitive;
    if (str == "defunct") return State::Defunct;
    if (str == "active") return State::Active;
    if (str == "armed") return State::Armed;
    if (str == "indeterminate") return State::Indeterminate;
    if (str == "vertical") return State::Vertical;
    if (str == "horizontal") return State::Horizontal;
    if (str == "required") return State::Required;
    if (str == "invalid") return State::Invalid;
    if (str == "multi_line") return State::MultiLine;
    if (str == "single_line") return State::SingleLine;
    if (str == "has_popup") return State::HasPopup;
    if (str == "selectable_text") return State::SelectableText;
    if (str == "editable") return State::Editable;
    if (str == "animated") return State::Animated;
    if (str == "transient") return State::Transient;
    return State::Count;
}

std::vector<State> StateSet::to_vector() const {
    std::vector<State> res;
    for (size_t i = 0; i < static_cast<size_t>(State::Count); ++i) {
        if (bits_.test(i)) {
            res.push_back(static_cast<State>(i));
        }
    }
    return res;
}

std::string StateSet::to_string() const {
    std::ostringstream oss;
    bool first = true;
    for (size_t i = 0; i < static_cast<size_t>(State::Count); ++i) {
        if (bits_.test(i)) {
            if (!first) oss << ", ";
            oss << state_to_string(static_cast<State>(i));
            first = false;
        }
    }
    return oss.str();
}

std::pair<uint32_t, uint32_t> StateSet::to_atspi_state_bitmask() const {
    uint32_t low = 0;
    uint32_t high = 0;

    auto set_atspi_bit = [&](uint32_t atspi_index) {
        if (atspi_index < 32) {
            low |= (1u << atspi_index);
        } else if (atspi_index < 64) {
            high |= (1u << (atspi_index - 32));
        }
    };

    if (has(State::Active)) set_atspi_bit(1);
    if (has(State::Armed)) set_atspi_bit(2);
    if (has(State::Busy)) set_atspi_bit(3);
    if (has(State::Checked)) set_atspi_bit(4);
    if (has(State::Collapsed)) set_atspi_bit(5);
    if (has(State::Defunct)) set_atspi_bit(6);
    if (has(State::Editable)) set_atspi_bit(7);
    // AT-SPI has no "disabled": a usable widget is ENABLED and SENSITIVE, and
    // Orca reports anything lacking them as unavailable.
    if (!has(State::Disabled)) {
        set_atspi_bit(8);   // ATSPI_STATE_ENABLED
        set_atspi_bit(24);  // ATSPI_STATE_SENSITIVE
    }
    if (has(State::Expanded)) set_atspi_bit(10);
    if (has(State::Focusable)) set_atspi_bit(11);
    if (has(State::Focused)) set_atspi_bit(12);
    if (has(State::Horizontal)) set_atspi_bit(14);
    if (has(State::Modal)) set_atspi_bit(16);
    if (has(State::MultiLine)) set_atspi_bit(17);
    if (has(State::MultiSelectable)) set_atspi_bit(18);
    if (has(State::Selectable)) set_atspi_bit(22);
    if (has(State::Selected)) set_atspi_bit(23);
    if (has(State::Sensitive) && !has(State::Disabled)) set_atspi_bit(24);
    if (has(State::Showing)) set_atspi_bit(25);
    if (has(State::SingleLine)) set_atspi_bit(26);
    if (has(State::Transient)) set_atspi_bit(28);
    if (has(State::Vertical)) set_atspi_bit(29);
    if (has(State::Visible)) set_atspi_bit(30);
    if (has(State::Indeterminate)) set_atspi_bit(32);
    if (has(State::Required)) set_atspi_bit(33);
    if (has(State::Animated)) set_atspi_bit(35);
    if (has(State::Invalid)) set_atspi_bit(36);
    if (has(State::SelectableText)) set_atspi_bit(38);
    if (has(State::HasPopup)) set_atspi_bit(42);
    if (has(State::ReadOnly)) set_atspi_bit(43);

    return {low, high};
}

} // namespace broa11y
