#include "broa11y/tree.h"

#include <algorithm>

namespace broa11y {

Tree::Tree() = default;
Tree::~Tree() = default;

Tree::Tree(Tree&& other) noexcept
    : root_id_(other.root_id_),
      focused_node_id_(other.focused_node_id_),
      next_id_(other.next_id_),
      nodes_(std::move(other.nodes_)),
      in_transaction_(other.in_transaction_),
      batched_events_(std::move(other.batched_events_)),
      transaction_backup_(std::move(other.transaction_backup_)),
      next_listener_id_(other.next_listener_id_),
      listeners_(std::move(other.listeners_)) {
    for (auto& [_, node] : nodes_) {
        if (node) {
            node->set_tree(this);
        }
    }
    other.root_id_ = kInvalidNodeId;
    other.focused_node_id_ = kInvalidNodeId;
}

Tree& Tree::operator=(Tree&& other) noexcept {
    if (this != &other) {
        root_id_ = other.root_id_;
        focused_node_id_ = other.focused_node_id_;
        next_id_ = other.next_id_;
        nodes_ = std::move(other.nodes_);
        in_transaction_ = other.in_transaction_;
        batched_events_ = std::move(other.batched_events_);
        transaction_backup_ = std::move(other.transaction_backup_);
        next_listener_id_ = other.next_listener_id_;
        listeners_ = std::move(other.listeners_);

        for (auto& [_, node] : nodes_) {
            if (node) {
                node->set_tree(this);
            }
        }
        other.root_id_ = kInvalidNodeId;
        other.focused_node_id_ = kInvalidNodeId;
    }
    return *this;
}

void Tree::set_root_id(NodeId id) {
    root_id_ = id;
}

Node* Tree::root() noexcept {
    return get_node(root_id_);
}

const Node* Tree::root() const noexcept {
    return get_node(root_id_);
}

Node* Tree::get_node(NodeId id) noexcept {
    if (id == kInvalidNodeId) return nullptr;
    auto it = nodes_.find(id);
    return (it != nodes_.end()) ? it->second.get() : nullptr;
}

const Node* Tree::get_node(NodeId id) const noexcept {
    if (id == kInvalidNodeId) return nullptr;
    auto it = nodes_.find(id);
    return (it != nodes_.end()) ? it->second.get() : nullptr;
}

bool Tree::contains_node(NodeId id) const noexcept {
    return nodes_.contains(id);
}

NodeId Tree::allocate_id() {
    while (next_id_ == kInvalidNodeId || nodes_.contains(next_id_)) {
        ++next_id_;
    }
    return next_id_++;
}

Node* Tree::create_node(NodeId id) {
    if (id == kInvalidNodeId) {
        id = allocate_id();
    }
    auto node = std::make_unique<Node>(id);
    Node* ptr = node.get();
    if (!add_node(std::move(node))) {
        return nullptr;
    }
    return ptr;
}

Node* Tree::create_node_with_role(Role role, NodeId id) {
    if (id == kInvalidNodeId) {
        id = allocate_id();
    }
    auto node = std::make_unique<Node>(id, role);
    Node* ptr = node.get();
    if (!add_node(std::move(node))) {
        return nullptr;
    }
    return ptr;
}

bool Tree::add_node(std::unique_ptr<Node> node, NodeId parent_id, size_t index) {
    if (!node) return false;
    NodeId id = node->id();
    if (id == kInvalidNodeId || nodes_.contains(id)) {
        return false;
    }

    node->set_tree(this);
    Node* ptr = node.get();
    nodes_[id] = std::move(node);

    if (in_transaction_ && transaction_backup_) {
        transaction_backup_->created_node_ids.push_back(id);
    }

    if (root_id_ == kInvalidNodeId && parent_id == kInvalidNodeId) {
        root_id_ = id;
    }

    if (parent_id != kInvalidNodeId) {
        Node* parent = get_node(parent_id);
        if (parent) {
            ptr->set_parent_id(parent_id);
            parent->add_child_id(id, index);
            notify_node_added(id, parent_id, index < parent->child_count() ? index : parent->child_count() - 1);
        }
    } else {
        notify_node_added(id, kInvalidNodeId, 0);
    }

    return true;
}

bool Tree::remove_node(NodeId id) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        return false;
    }

    Node* node = it->second.get();
    NodeId parent_id = node->parent_id();
    size_t old_index = 0;

    if (parent_id != kInvalidNodeId) {
        Node* parent = get_node(parent_id);
        if (parent) {
            int32_t idx = node->index_in_parent();
            if (idx >= 0) old_index = static_cast<size_t>(idx);
            parent->remove_child_id(id);
        }
    }

    if (focused_node_id_ == id) {
        clear_focus();
    }
    if (root_id_ == id) {
        root_id_ = kInvalidNodeId;
    }

    // Recursively remove children
    std::vector<NodeId> children_copy = node->children_ids();
    for (NodeId child_id : children_copy) {
        remove_node(child_id);
    }

    notify_node_removed(id, parent_id, old_index);
    nodes_.erase(it);
    return true;
}

bool Tree::reparent_node(NodeId id, NodeId new_parent_id, size_t index) {
    Node* node = get_node(id);
    if (!node) return false;
    if (id == new_parent_id) return false;

    NodeId old_parent_id = node->parent_id();
    if (old_parent_id != kInvalidNodeId) {
        Node* old_parent = get_node(old_parent_id);
        if (old_parent) {
            int32_t old_idx = node->index_in_parent();
            old_parent->remove_child_id(id);
            notify_node_removed(id, old_parent_id, old_idx >= 0 ? static_cast<size_t>(old_idx) : 0);
        }
    }

    node->set_parent_id(new_parent_id);
    if (new_parent_id != kInvalidNodeId) {
        Node* new_parent = get_node(new_parent_id);
        if (new_parent) {
            new_parent->add_child_id(id, index);
            size_t actual_idx = index < new_parent->child_count() ? index : new_parent->child_count() - 1;
            notify_node_added(id, new_parent_id, actual_idx);
        }
    }
    return true;
}

Node* Tree::focused_node() noexcept {
    return get_node(focused_node_id_);
}

const Node* Tree::focused_node() const noexcept {
    return get_node(focused_node_id_);
}

bool Tree::set_focus(NodeId id) {
    if (id == focused_node_id_) return true;

    Node* new_focus = (id != kInvalidNodeId) ? get_node(id) : nullptr;
    if (id != kInvalidNodeId && !new_focus) return false;

    NodeId prev_id = focused_node_id_;
    Node* prev_focus = get_node(prev_id);
    if (prev_focus) {
        prev_focus->states().reset(State::Focused);
        notify_state_changed(prev_id, State::Focused, false);
    }

    focused_node_id_ = id;
    if (new_focus) {
        new_focus->states().set(State::Focused, true);
        notify_state_changed(id, State::Focused, true);
    }

    Event ev;
    ev.type = EventType::FocusChanged;
    ev.node_id = id;
    ev.payload = FocusChangedPayload{
        .previous_focused_id = prev_id,
        .current_focused_id = id
    };
    dispatch_event(ev);
    return true;
}

void Tree::clear_focus() {
    set_focus(kInvalidNodeId);
}

NodeId Tree::hit_test_from(NodeId start_id, PointF point) const {
    const Node* node = get_node(start_id);
    if (!node) return kInvalidNodeId;

    if (!node->bounds().contains(point)) {
        return kInvalidNodeId;
    }

    // Check children in reverse order (topmost first)
    const auto& children = node->children_ids();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        NodeId child_id = *it;
        NodeId hit = hit_test_from(child_id, point);
        if (hit != kInvalidNodeId) {
            return hit;
        }
    }

    return start_id;
}

NodeId Tree::hit_test(PointF point) const {
    if (root_id_ == kInvalidNodeId) return kInvalidNodeId;
    return hit_test_from(root_id_, point);
}

std::vector<Node*> Tree::find_by_role(Role role) const {
    std::vector<Node*> res;
    for (const auto& [_, node] : nodes_) {
        if (node && node->role() == role) {
            res.push_back(node.get());
        }
    }
    return res;
}

std::vector<Node*> Tree::find_by_name(std::string_view name) const {
    std::vector<Node*> res;
    for (const auto& [_, node] : nodes_) {
        if (node && node->name() == name) {
            res.push_back(node.get());
        }
    }
    return res;
}

void Tree::announce(std::string_view message, AnnouncementPriority priority, NodeId origin_id) {
    Event ev;
    ev.type = EventType::Announcement;
    ev.node_id = (origin_id != kInvalidNodeId) ? origin_id : root_id_;
    ev.payload = AnnouncementPayload{
        .message = std::string(message),
        .priority = priority
    };
    dispatch_event(ev);
}

EventListenerId Tree::add_listener(EventListener listener) {
    EventListenerId id = next_listener_id_++;
    listeners_[id] = std::move(listener);
    return id;
}

void Tree::remove_listener(EventListenerId id) {
    listeners_.erase(id);
}

void Tree::dispatch_event(const Event& event) {
    if (in_transaction_) {
        batched_events_.push_back(event);
        return;
    }
    for (const auto& [_, cb] : listeners_) {
        if (cb) {
            cb(event);
        }
    }
}

void Tree::notify_node_added(NodeId id, NodeId parent_id, size_t index) {
    Event ev;
    ev.type = EventType::NodeAdded;
    ev.node_id = id;
    dispatch_event(ev);

    if (parent_id != kInvalidNodeId) {
        Event c_ev;
        c_ev.type = EventType::ChildrenChanged;
        c_ev.node_id = parent_id;
        c_ev.payload = ChildrenChangedPayload{
            .change_type = ChildrenChangeType::ChildAdded,
            .child_id = id,
            .index = index
        };
        dispatch_event(c_ev);
    }
}

void Tree::notify_node_removed(NodeId id, NodeId parent_id, size_t index) {
    Event ev;
    ev.type = EventType::NodeRemoved;
    ev.node_id = id;
    dispatch_event(ev);

    if (parent_id != kInvalidNodeId) {
        Event c_ev;
        c_ev.type = EventType::ChildrenChanged;
        c_ev.node_id = parent_id;
        c_ev.payload = ChildrenChangedPayload{
            .change_type = ChildrenChangeType::ChildRemoved,
            .child_id = id,
            .index = index
        };
        dispatch_event(c_ev);
    }
}

void Tree::notify_property_changed(NodeId id, std::string_view prop, std::string_view old_val, std::string_view new_val) {
    Event ev;
    ev.type = EventType::PropertyChanged;
    ev.node_id = id;
    ev.payload = PropertyChangedPayload{
        .property_name = std::string(prop),
        .old_value = std::string(old_val),
        .new_value = std::string(new_val)
    };
    dispatch_event(ev);
}

void Tree::notify_state_changed(NodeId id, State state, bool enabled) {
    Event ev;
    ev.type = EventType::StateChanged;
    ev.node_id = id;
    ev.payload = StateChangedPayload{
        .state = state,
        .enabled = enabled
    };
    dispatch_event(ev);
}

void Tree::notify_bounds_changed(NodeId id, const RectF& old_bounds, const RectF& new_bounds) {
    Event ev;
    ev.type = EventType::BoundsChanged;
    ev.node_id = id;
    ev.payload = BoundsChangedPayload{
        .old_bounds = old_bounds,
        .new_bounds = new_bounds
    };
    dispatch_event(ev);
}

void Tree::notify_value_changed(NodeId id, const ValueRange& old_val, const ValueRange& new_val) {
    Event ev;
    ev.type = EventType::ValueChanged;
    ev.node_id = id;
    ev.payload = ValueChangedPayload{
        .old_value = old_val,
        .new_value = new_val
    };
    dispatch_event(ev);
}

void Tree::notify_caret_moved(NodeId id, int32_t old_offset, int32_t new_offset) {
    Event ev;
    ev.type = EventType::CaretMoved;
    ev.node_id = id;
    ev.payload = CaretMovedPayload{
        .old_offset = old_offset,
        .new_offset = new_offset
    };
    dispatch_event(ev);
}

void Tree::notify_selection_changed(NodeId id, const TextRange& selection) {
    Event ev;
    ev.type = EventType::TextSelectionChanged;
    ev.node_id = id;
    ev.payload = TextSelectionPayload{
        .selection = selection
    };
    dispatch_event(ev);
}

} // namespace broa11y
