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

    // 3b. Rollback brings back a removed subtree, handler included
    {
        auto* group = tree.create_node_with_role(broa11y::Role::Group, 4);
        tree.reparent_node(4, 1);
        auto* leaf = tree.create_node_with_role(broa11y::Role::Button, 5);
        tree.reparent_node(5, 4);
        leaf->set_action_handler([](broa11y::NodeId, std::string_view, const broa11y::ActionParams&) {
            return true;
        });
        group->set_name("Group");
        received_events.clear();

        broa11y::TreeTransaction txn(tree);
        CHECK(tree.remove_node(4));
        CHECK(tree.get_node(4) == nullptr);
        CHECK(tree.get_node(5) == nullptr);
        txn.rollback();

        REQUIRE(tree.get_node(4) != nullptr);
        REQUIRE(tree.get_node(5) != nullptr);
        CHECK_EQ(tree.get_node(4)->name(), "Group");
        CHECK_EQ(tree.get_node(4)->parent_id(), 1u);
        CHECK_EQ(tree.get_node(5)->parent_id(), 4u);
        CHECK_EQ(root->child_count(), 2u);
        CHECK(tree.get_node(5)->perform_action(broa11y::kActionActivate));
        CHECK_EQ(received_events.size(), 0u);
        CHECK(tree.remove_node(4));
        CHECK_EQ(root->child_count(), 1u);
    }

    // 4. RAII automatic rollback on exit without commit
    {
        broa11y::TreeTransaction txn(tree);
        root->set_name("Never Committed");
        // No txn.commit() called
    }
    CHECK_EQ(root->name(), "Transaction Name");

    return bstest::finish("test_transaction");
}
