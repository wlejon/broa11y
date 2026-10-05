#pragma once

#include "broa11y/events.h"
#include "broa11y/node.h"
#include "broa11y/types.h"

#include <functional>
#include <memory>
#include <queue>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace broa11y {

class TreeTransaction;

class Tree {
public:
    Tree();
    ~Tree();

    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;
    Tree(Tree&&) noexcept;
    Tree& operator=(Tree&&) noexcept;

    [[nodiscard]] NodeId root_id() const noexcept { return root_id_; }
    void set_root_id(NodeId id);

    [[nodiscard]] Node* root() noexcept;
    [[nodiscard]] const Node* root() const noexcept;

    [[nodiscard]] Node* get_node(NodeId id) noexcept;
    [[nodiscard]] const Node* get_node(NodeId id) const noexcept;
    [[nodiscard]] bool contains_node(NodeId id) const noexcept;
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }

    Node* create_node(NodeId id = kInvalidNodeId);
    Node* create_node_with_role(Role role, NodeId id = kInvalidNodeId);
    bool add_node(std::unique_ptr<Node> node, NodeId parent_id = kInvalidNodeId, size_t index = static_cast<size_t>(-1));
    bool remove_node(NodeId id);
    bool reparent_node(NodeId id, NodeId new_parent_id, size_t index = static_cast<size_t>(-1));

    [[nodiscard]] NodeId focused_node_id() const noexcept { return focused_node_id_; }
    [[nodiscard]] Node* focused_node() noexcept;
    [[nodiscard]] const Node* focused_node() const noexcept;
    bool set_focus(NodeId id);
    void clear_focus();

    [[nodiscard]] NodeId hit_test(PointF point) const;
    [[nodiscard]] NodeId hit_test_from(NodeId start_id, PointF point) const;

    [[nodiscard]] std::vector<Node*> find_by_role(Role role) const;
    [[nodiscard]] std::vector<Node*> find_by_name(std::string_view name) const;

    template <typename Callback>
    void for_each_dfs(Callback&& cb) const {
        if (root_id_ == kInvalidNodeId) return;
        traverse_dfs(root_id_, cb);
    }

    template <typename Callback>
    void for_each_bfs(Callback&& cb) const {
        if (root_id_ == kInvalidNodeId) return;
        std::queue<NodeId> q;
        q.push(root_id_);
        while (!q.empty()) {
            NodeId current = q.front();
            q.pop();
            const Node* node = get_node(current);
            if (!node) continue;
            if (!cb(node)) return;
            for (NodeId child_id : node->children_ids()) {
                q.push(child_id);
            }
        }
    }

    void announce(std::string_view message,
                  AnnouncementPriority priority = AnnouncementPriority::Polite,
                  NodeId origin_id = kInvalidNodeId);

    // Transaction & Batching
    void begin_transaction();
    void commit_transaction();
    void rollback_transaction();
    [[nodiscard]] bool in_transaction() const noexcept { return in_transaction_; }

    EventListenerId add_listener(EventListener listener);
    void remove_listener(EventListenerId id);
    void dispatch_event(const Event& event);

    [[nodiscard]] NodeId allocate_id();

private:
    friend class Node;
    friend class TreeTransaction;

    void notify_node_added(NodeId id, NodeId parent_id, size_t index);
    void notify_node_removed(NodeId id, NodeId parent_id, size_t index);
    void notify_property_changed(NodeId id, std::string_view prop, std::string_view old_val, std::string_view new_val);
    void notify_state_changed(NodeId id, State state, bool enabled);
    void notify_bounds_changed(NodeId id, const RectF& old_bounds, const RectF& new_bounds);
    void notify_value_changed(NodeId id, const ValueRange& old_val, const ValueRange& new_val);
    void notify_caret_moved(NodeId id, int32_t old_offset, int32_t new_offset);
    void notify_selection_changed(NodeId id, const TextRange& selection);

    template <typename Callback>
    bool traverse_dfs(NodeId id, Callback&& cb) const {
        const Node* node = get_node(id);
        if (!node) return true;
        if (!cb(node)) return false;
        for (NodeId child_id : node->children_ids()) {
            if (!traverse_dfs(child_id, cb)) return false;
        }
        return true;
    }

    struct TransactionBackup {
        std::unordered_map<NodeId, NodeData> node_data;
        std::unordered_map<NodeId, NodeId> node_parents;
        std::unordered_map<NodeId, std::vector<NodeId>> node_children;
        NodeId root_id = kInvalidNodeId;
        NodeId focused_node_id = kInvalidNodeId;
        std::vector<NodeId> created_node_ids;
    };

    NodeId root_id_ = kInvalidNodeId;
    NodeId focused_node_id_ = kInvalidNodeId;
    NodeId next_id_ = 100;
    std::unordered_map<NodeId, std::unique_ptr<Node>> nodes_;

    bool in_transaction_ = false;
    std::vector<Event> batched_events_;
    std::unique_ptr<TransactionBackup> transaction_backup_;

    EventListenerId next_listener_id_ = 1;
    std::unordered_map<EventListenerId, EventListener> listeners_;
};

class TreeTransaction {
public:
    explicit TreeTransaction(Tree& tree) : tree_(&tree) {
        tree_->begin_transaction();
    }

    ~TreeTransaction() {
        if (tree_ && !committed_) {
            tree_->rollback_transaction();
        }
    }

    TreeTransaction(const TreeTransaction&) = delete;
    TreeTransaction& operator=(const TreeTransaction&) = delete;

    TreeTransaction(TreeTransaction&& other) noexcept
        : tree_(other.tree_), committed_(other.committed_) {
        other.tree_ = nullptr;
    }

    TreeTransaction& operator=(TreeTransaction&& other) noexcept {
        if (this != &other) {
            if (tree_ && !committed_) {
                tree_->rollback_transaction();
            }
            tree_ = other.tree_;
            committed_ = other.committed_;
            other.tree_ = nullptr;
        }
        return *this;
    }

    void commit() {
        if (tree_ && !committed_) {
            tree_->commit_transaction();
            committed_ = true;
        }
    }

    void rollback() {
        if (tree_ && !committed_) {
            tree_->rollback_transaction();
            committed_ = true;
        }
    }

private:
    Tree* tree_ = nullptr;
    bool committed_ = false;
};

} // namespace broa11y
