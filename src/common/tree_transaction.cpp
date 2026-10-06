#include "broa11y/tree.h"

namespace broa11y {

void Tree::begin_transaction() {
    if (in_transaction_) return;
    in_transaction_ = true;
    batched_events_.clear();

    transaction_backup_ = std::make_unique<TransactionBackup>();
    transaction_backup_->root_id = root_id_;
    transaction_backup_->focused_node_id = focused_node_id_;

    for (const auto& [id, node] : nodes_) {
        if (node) {
            transaction_backup_->node_data[id] = node->data();
            transaction_backup_->node_parents[id] = node->parent_id();
            transaction_backup_->node_children[id] = node->children_ids();
        }
    }
}

void Tree::commit_transaction() {
    if (!in_transaction_) return;
    in_transaction_ = false;
    transaction_backup_.reset();

    std::vector<Event> events = std::move(batched_events_);
    batched_events_.clear();

    for (const auto& ev : events) {
        for (const auto& [_, cb] : listeners_) {
            if (cb) {
                cb(ev);
            }
        }
    }
}

void Tree::rollback_transaction() {
    if (!in_transaction_) return;
    in_transaction_ = false;
    batched_events_.clear();

    if (!transaction_backup_) return;

    // Erase the nodes the transaction created, then bring back the ones it
    // removed (a node both created and removed in it is simply dropped).
    for (NodeId created_id : transaction_backup_->created_node_ids) {
        nodes_.erase(created_id);
    }
    for (auto& removed : transaction_backup_->removed_nodes) {
        NodeId rid = removed->id();
        if (transaction_backup_->node_data.contains(rid) && !nodes_.contains(rid)) {
            nodes_[rid] = std::move(removed);
        }
    }

    // Restore root and focus
    root_id_ = transaction_backup_->root_id;
    focused_node_id_ = transaction_backup_->focused_node_id;

    // Restore node states and connections
    for (const auto& [id, data] : transaction_backup_->node_data) {
        auto it = nodes_.find(id);
        if (it != nodes_.end() && it->second) {
            it->second->data_ = data;
            it->second->parent_id_ = transaction_backup_->node_parents[id];
            it->second->children_ids_ = transaction_backup_->node_children[id];
            it->second->set_tree(this);
        }
    }

    transaction_backup_.reset();
}

} // namespace broa11y
