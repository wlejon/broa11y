#include "broa11y/node.h"
#include "broa11y/tree.h"

#include <algorithm>

namespace broa11y {

Node::Node(NodeId id, Role role) {
    data_.id = id;
    data_.role = role;
}

Node::Node(NodeData data) : data_(std::move(data)) {}

void Node::set_role(Role role) {
    if (data_.role != role) {
        auto old_str = role_to_string(data_.role);
        data_.role = role;
        if (tree_) {
            tree_->notify_property_changed(data_.id, "role", old_str, role_to_string(role));
        }
    }
}

void Node::set_state(State s, bool value) {
    bool current = data_.states.has(s);
    if (current != value) {
        data_.states.set(s, value);
        if (tree_) {
            tree_->notify_state_changed(data_.id, s, value);
        }
    }
}

void Node::set_bounds(const RectF& bounds) {
    if (data_.bounds != bounds) {
        RectF old = data_.bounds;
        data_.bounds = bounds;
        if (tree_) {
            tree_->notify_bounds_changed(data_.id, old, bounds);
        }
    }
}

void Node::set_name(std::string_view name) {
    if (data_.name != name) {
        std::string old = std::move(data_.name);
        data_.name = std::string(name);
        if (tree_) {
            tree_->notify_property_changed(data_.id, "name", old, data_.name);
        }
    }
}

void Node::set_description(std::string_view desc) {
    if (data_.description != desc) {
        std::string old = std::move(data_.description);
        data_.description = std::string(desc);
        if (tree_) {
            tree_->notify_property_changed(data_.id, "description", old, data_.description);
        }
    }
}

void Node::set_value(const ValueRange& value) {
    ValueRange old = data_.value.value_or(ValueRange{});
    bool had_val = data_.value.has_value();
    data_.value = value;
    if (tree_ && (!had_val || old != value)) {
        tree_->notify_value_changed(data_.id, old, value);
    }
}

void Node::clear_value() {
    if (data_.value.has_value()) {
        ValueRange old = *data_.value;
        data_.value.reset();
        if (tree_) {
            tree_->notify_value_changed(data_.id, old, ValueRange{});
        }
    }
}

void Node::set_text(std::string_view text) {
    if (data_.text != text) {
        std::string old = std::move(data_.text);
        data_.text = std::string(text);
        if (tree_) {
            tree_->notify_property_changed(data_.id, "text", old, data_.text);
        }
    }
}

void Node::set_caret_offset(int32_t offset) {
    if (data_.caret_offset != offset) {
        int32_t old = data_.caret_offset;
        data_.caret_offset = offset;
        if (tree_) {
            tree_->notify_caret_moved(data_.id, old, offset);
        }
    }
}

void Node::set_selection(const TextRange& selection) {
    if (data_.selection != selection) {
        data_.selection = selection;
        if (tree_) {
            tree_->notify_selection_changed(data_.id, selection);
        }
    }
}

void Node::set_attribute(std::string_view key, std::string_view value) {
    data_.attributes[std::string(key)] = std::string(value);
}

std::optional<std::string> Node::get_attribute(std::string_view key) const {
    auto it = data_.attributes.find(std::string(key));
    if (it != data_.attributes.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Node::add_relation(RelationType type, NodeId target_id) {
    auto& list = data_.relations[static_cast<uint32_t>(type)];
    if (std::find(list.begin(), list.end(), target_id) == list.end()) {
        list.push_back(target_id);
    }
}

void Node::remove_relation(RelationType type, NodeId target_id) {
    auto it = data_.relations.find(static_cast<uint32_t>(type));
    if (it != data_.relations.end()) {
        auto& list = it->second;
        list.erase(std::remove(list.begin(), list.end(), target_id), list.end());
    }
}

std::vector<NodeId> Node::get_relations(RelationType type) const {
    auto it = data_.relations.find(static_cast<uint32_t>(type));
    if (it != data_.relations.end()) {
        return it->second;
    }
    return {};
}

void Node::add_action(ActionDescriptor action) {
    for (auto& existing : data_.actions) {
        if (existing.name == action.name) {
            existing = std::move(action);
            return;
        }
    }
    data_.actions.push_back(std::move(action));
}

bool Node::perform_action(std::string_view action_name, const ActionParams& params) {
    if (action_handler_) {
        return action_handler_(data_.id, action_name, params);
    }
    for (const auto& a : data_.actions) {
        if (a.name == action_name) {
            return true;
        }
    }
    return false;
}

NodeId Node::child_at(size_t index) const noexcept {
    if (index < children_ids_.size()) {
        return children_ids_[index];
    }
    return kInvalidNodeId;
}

int32_t Node::index_in_parent() const noexcept {
    if (!tree_ || parent_id_ == kInvalidNodeId) {
        return -1;
    }
    const Node* p = tree_->get_node(parent_id_);
    if (!p) return -1;
    for (size_t i = 0; i < p->children_ids_.size(); ++i) {
        if (p->children_ids_[i] == data_.id) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

Node* Node::parent() const noexcept {
    return (tree_ && parent_id_ != kInvalidNodeId) ? tree_->get_node(parent_id_) : nullptr;
}

Node* Node::first_child() const noexcept {
    if (children_ids_.empty() || !tree_) return nullptr;
    return tree_->get_node(children_ids_.front());
}

Node* Node::last_child() const noexcept {
    if (children_ids_.empty() || !tree_) return nullptr;
    return tree_->get_node(children_ids_.back());
}

Node* Node::next_sibling() const noexcept {
    if (!tree_ || parent_id_ == kInvalidNodeId) return nullptr;
    const Node* p = tree_->get_node(parent_id_);
    if (!p) return nullptr;
    int32_t idx = index_in_parent();
    if (idx >= 0 && static_cast<size_t>(idx + 1) < p->children_ids_.size()) {
        return tree_->get_node(p->children_ids_[static_cast<size_t>(idx + 1)]);
    }
    return nullptr;
}

Node* Node::previous_sibling() const noexcept {
    if (!tree_ || parent_id_ == kInvalidNodeId) return nullptr;
    const Node* p = tree_->get_node(parent_id_);
    if (!p) return nullptr;
    int32_t idx = index_in_parent();
    if (idx > 0) {
        return tree_->get_node(p->children_ids_[static_cast<size_t>(idx - 1)]);
    }
    return nullptr;
}

Node* Node::child_node_at(size_t index) const noexcept {
    if (!tree_ || index >= children_ids_.size()) return nullptr;
    return tree_->get_node(children_ids_[index]);
}

void Node::add_child_id(NodeId cid, size_t index) {
    if (index >= children_ids_.size()) {
        children_ids_.push_back(cid);
    } else {
        children_ids_.insert(children_ids_.begin() + static_cast<ptrdiff_t>(index), cid);
    }
}

bool Node::remove_child_id(NodeId cid) {
    auto it = std::find(children_ids_.begin(), children_ids_.end(), cid);
    if (it != children_ids_.end()) {
        children_ids_.erase(it);
        return true;
    }
    return false;
}

} // namespace broa11y
