#include "uia_node_provider.h"
#include "broa11y/role.h"

#include <iomanip>
#include <sstream>

namespace broa11y::uia {

UiaNodeProvider::UiaNodeProvider(Node* node) : node_(node) {}

NodeId UiaNodeProvider::id() const noexcept {
    return node_ ? node_->id() : kInvalidNodeId;
}

std::string UiaNodeProvider::get_property_value(int32_t property_id) const {
    if (!node_) return {};

    switch (property_id) {
        case kControlTypePropertyId:
            return std::to_string(role_to_uia_control_type(node_->role()));
        case kNamePropertyId:
            return node_->name();
        case kBoundingRectanglePropertyId: {
            const auto& b = node_->bounds();
            std::ostringstream oss;
            oss << "[" << b.x << ", " << b.y << ", " << b.width << ", " << b.height << "]";
            return oss.str();
        }
        case kIsEnabledPropertyId:
            return !node_->has_state(State::Disabled) ? "true" : "false";
        case kHasKeyboardFocusPropertyId:
            return node_->has_state(State::Focused) ? "true" : "false";
        case kIsKeyboardFocusablePropertyId:
            return node_->has_state(State::Focusable) ? "true" : "false";
        case kHelpTextPropertyId:
            return node_->description();
        case kValueValuePropertyId:
            return node_->text();
        case kRangeValueValuePropertyId:
            if (node_->value().has_value()) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << node_->value()->current;
                return oss.str();
            }
            return "0.0";
        case kRangeValueMinimumPropertyId:
            if (node_->value().has_value()) {
                return std::to_string(node_->value()->minimum);
            }
            return "0.0";
        case kRangeValueMaximumPropertyId:
            if (node_->value().has_value()) {
                return std::to_string(node_->value()->maximum);
            }
            return "100.0";
        default:
            return {};
    }
}

bool UiaNodeProvider::is_pattern_supported(int32_t pattern_id) const {
    if (!node_) return false;
    Role r = node_->role();

    switch (pattern_id) {
        case kInvokePatternId:
            return r == Role::Button || r == Role::MenuItem || r == Role::Link;
        case kTogglePatternId:
            return r == Role::CheckBox || r == Role::RadioButton;
        case kValuePatternId:
            return r == Role::TextInput || r == Role::Terminal || r == Role::ComboBox;
        case kRangeValuePatternId:
            return r == Role::Slider || r == Role::ProgressBar || r == Role::ScrollBar || r == Role::SpinButton;
        case kTextPatternId:
            return r == Role::TextInput || r == Role::Terminal || r == Role::Document;
        case kSelectionPatternId:
            return r == Role::List || r == Role::Tree || r == Role::TabList;
        case kExpandCollapsePatternId:
            return r == Role::TreeItem || r == Role::ComboBox;
        default:
            return false;
    }
}

NodeId UiaNodeProvider::navigate(NavigateDirection direction) const {
    if (!node_) return kInvalidNodeId;

    switch (direction) {
        case NavigateDirection::Parent: {
            Node* p = node_->parent();
            return p ? p->id() : kInvalidNodeId;
        }
        case NavigateDirection::NextSibling: {
            Node* s = node_->next_sibling();
            return s ? s->id() : kInvalidNodeId;
        }
        case NavigateDirection::PreviousSibling: {
            Node* s = node_->previous_sibling();
            return s ? s->id() : kInvalidNodeId;
        }
        case NavigateDirection::FirstChild: {
            Node* c = node_->first_child();
            return c ? c->id() : kInvalidNodeId;
        }
        case NavigateDirection::LastChild: {
            Node* c = node_->last_child();
            return c ? c->id() : kInvalidNodeId;
        }
    }
    return kInvalidNodeId;
}

bool UiaNodeProvider::execute_action(int32_t pattern_id, std::string_view action_name) {
    if (!node_ || !is_pattern_supported(pattern_id)) return false;

    if (pattern_id == kInvokePatternId) {
        return node_->perform_action(kActionActivate);
    } else if (pattern_id == kTogglePatternId) {
        bool cur = node_->has_state(State::Checked);
        node_->set_state(State::Checked, !cur);
        return true;
    } else if (pattern_id == kValuePatternId) {
        if (action_name == "set") {
            return node_->perform_action(kActionSetValue);
        }
    }
    return node_->perform_action(action_name);
}

} // namespace broa11y::uia
