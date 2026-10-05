#include "check.h"
#include "broa11y/tree.h"

int main() {
    broa11y::Tree tree;
    auto* root = tree.create_node_with_role(broa11y::Role::Window, 1);
    root->set_name("Initial Window");

    std::vector<broa11y::EventType> received_events;
    tree.add_listener([&](const broa11y::Event& ev) {
        received_events.push_back(ev.type);
    });

    // 1. Immediate dispatch outside transaction
    root->set_name("Renamed Window");
    CHECK_EQ(received_events.size(), 1u);
    CHECK(received_events[0] == broa11y::EventType::PropertyChanged);
    received_events.clear();

    // 2. Commit transaction
    {
        broa11y::TreeTransaction txn(tree);
        root->set_name("Transaction Name");
        root->set_state(broa11y::State::Modal, true);
        auto* child = tree.create_node_with_role(broa11y::Role::Button, 2);
        child->set_name("Txn Button");
        tree.reparent_node(2, 1);

        // No events should be fired while transaction is pending
        CHECK_EQ(received_events.size(), 0u);
        txn.commit();
    }

    // Now all events should have fired upon commit
    CHECK(received_events.size() >= 3u);
    CHECK_EQ(root->name(), "Transaction Name");
    CHECK(root->has_state(broa11y::State::Modal));
    CHECK(tree.get_node(2) != nullptr);
    received_events.clear();

    // 3. Rollback transaction
    {
        broa11y::TreeTransaction txn(tree);
        root->set_name("Discarded Name");
        root->set_state(broa11y::State::Modal, false);
        auto* discarded = tree.create_node_with_role(broa11y::Role::CheckBox, 3);
        discarded->set_name("Should Be Deleted");
        tree.reparent_node(3, 1);

        CHECK_EQ(received_events.size(), 0u);
        txn.rollback();
    }

    // After rollback: events discarded, changes reverted, created node deleted
    CHECK_EQ(received_events.size(), 0u);
    CHECK_EQ(root->name(), "Transaction Name");
    CHECK(root->has_state(broa11y::State::Modal));
    CHECK_EQ(tree.get_node(3), nullptr);
    CHECK_EQ(root->child_count(), 1u); // Only node 2 exists

    // 4. RAII automatic rollback on exit without commit
    {
        broa11y::TreeTransaction txn(tree);
        root->set_name("Never Committed");
        // No txn.commit() called
    }
    CHECK_EQ(root->name(), "Transaction Name");

    return check::finish("test_transaction");
}
