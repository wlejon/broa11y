#include "mac_accessible_element.h"
#include "broa11y/role.h"

#include <iomanip>
#include <sstream>

namespace broa11y::mac {

MacAccessibleElement::MacAccessibleElement(Node* node) : node_(node) {}

NodeId MacAccessibleElement::id() const noexcept {
    return node_ ? node_->id() : kInvalidNodeId;
}

std::string MacAccessibleElement::get_attribute(std::string_view attribute) const {
    if (!node_) return {};

    if (attribute == kRoleAttribute) {
        return std::string(role_to_mac_role(node_->role()));
    } else if (attribute == kSubroleAttribute) {
        return std::string(role_to_mac_subrole(node_->role()));
    } else if (attribute == kTitleAttribute) {
        return node_->name();
    } else if (attribute == kDescriptionAttribute) {
        return node_->description();
    } else if (attribute == kValueAttribute) {
        if (!node_->text().empty()) {
            return node_->text();
        }
        if (node_->value().has_value()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << node_->value()->current;
            return oss.str();
        }
        return {};
    } else if (attribute == kPositionAttribute) {
        const auto& b = node_->bounds();
        std::ostringstream oss;
        oss << "(" << b.x << ", " << b.y << ")";
        return oss.str();
    } else if (attribute == kSizeAttribute) {
        const auto& b = node_->bounds();
        std::ostringstream oss;
        oss << "(" << b.width << ", " << b.height << ")";
        return oss.str();
    } else if (attribute == kEnabledAttribute) {
        return !node_->has_state(State::Disabled) ? "true" : "false";
    } else if (attribute == kFocusedAttribute) {
        return node_->has_state(State::Focused) ? "true" : "false";
    } else if (attribute == kChildrenAttribute) {
        return std::to_string(node_->child_count());
    }

    return {};
}

std::string MacAccessibleElement::get_parameterized_attribute(std::string_view attribute,
                                                             std::string_view parameter) const {
    if (!node_) return {};

    if (attribute == kStringForRangeAttribute) {
        // parameter format: "start,length"
        auto comma = parameter.find(',');
        if (comma != std::string_view::npos) {
            int start = std::stoi(std::string(parameter.substr(0, comma)));
            int len = std::stoi(std::string(parameter.substr(comma + 1)));
            const std::string& txt = node_->text();
            if (start >= 0 && start < static_cast<int>(txt.size())) {
                return txt.substr(static_cast<size_t>(start), static_cast<size_t>(len));
            }
        }
    }
    return {};
}

bool MacAccessibleElement::perform_action(std::string_view action) {
    if (!node_) return false;

    if (action == kPressAction) {
        return node_->perform_action(kActionActivate);
    } else if (action == kIncrementAction && node_->value().has_value()) {
        auto v = *node_->value();
        v.current = std::min(v.current + v.step, v.maximum);
        node_->set_value(v);
        return true;
    } else if (action == kDecrementAction && node_->value().has_value()) {
        auto v = *node_->value();
        v.current = std::max(v.current - v.step, v.minimum);
        node_->set_value(v);
        return true;
    }
    return node_->perform_action(action);
}

} // namespace broa11y::mac
