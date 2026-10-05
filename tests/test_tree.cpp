#include "check.h"
#include "broa11y/tree.h"

int main() {
    broa11y::Tree tree;

    // 1. Initial state
    CHECK_EQ(tree.root_id(), broa11y::kInvalidNodeId);
    CHECK_EQ(tree.node_count(), 0u);
    CHECK(tree.root() == nullptr);

    // 2. Create Root window
    auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
    CHECK(window != nullptr);
    CHECK_EQ(tree.root_id(), 1u);
    CHECK_EQ(tree.node_count(), 1u);
    CHECK(tree.root() == window);
    window->set_name("Main Window");
    window->set_bounds({0, 0, 800, 600});

    // 3. Create children
    auto* panel = tree.create_node_with_role(broa11y::Role::Panel, 2);
    panel->set_name("Content Panel");
    panel->set_bounds({10, 10, 780, 580});
    tree.reparent_node(2, 1);

    auto* button1 = tree.create_node_with_role(broa11y::Role::Button, 3);
    button1->set_name("OK");
    button1->set_bounds({20, 20, 100, 30});
    button1->set_state(broa11y::State::Focusable, true);
    tree.reparent_node(3, 2);

    auto* button2 = tree.create_node_with_role(broa11y::Role::Button, 4);
    button2->set_name("Cancel");
    button2->set_bounds({130, 20, 100, 30});
    button2->set_state(broa11y::State::Focusable, true);
    tree.reparent_node(4, 2);

    // 4. Test hierarchy and navigation
    CHECK_EQ(panel->parent_id(), 1u);
    CHECK(panel->parent() == window);
    CHECK_EQ(panel->child_count(), 2u);
    CHECK_EQ(panel->child_at(0), 3u);
    CHECK_EQ(panel->child_at(1), 4u);
    CHECK(panel->first_child() == button1);
    CHECK(panel->last_child() == button2);

    CHECK(button1->parent() == panel);
    CHECK(button1->next_sibling() == button2);
    CHECK(button1->previous_sibling() == nullptr);
    CHECK_EQ(button1->index_in_parent(), 0);

    CHECK(button2->previous_sibling() == button1);
    CHECK(button2->next_sibling() == nullptr);
    CHECK_EQ(button2->index_in_parent(), 1);

    // 5. Test Hit testing
    CHECK_EQ(tree.hit_test({0, 0}), 1u); // Window
    CHECK_EQ(tree.hit_test({15, 15}), 2u); // Panel
    CHECK_EQ(tree.hit_test({25, 25}), 3u); // button1
    CHECK_EQ(tree.hit_test({150, 30}), 4u); // button2
    CHECK_EQ(tree.hit_test({900, 900}), broa11y::kInvalidNodeId); // Outside

    // 6. Test Find by role & name
    auto buttons = tree.find_by_role(broa11y::Role::Button);
    CHECK_EQ(buttons.size(), 2u);

    auto ok_btn = tree.find_by_name("OK");
    CHECK_EQ(ok_btn.size(), 1u);
    CHECK_EQ(ok_btn[0]->id(), 3u);

    // 7. Test Focus management
    CHECK_EQ(tree.focused_node_id(), broa11y::kInvalidNodeId);
    bool focus_ok = tree.set_focus(3);
    CHECK(focus_ok);
    CHECK_EQ(tree.focused_node_id(), 3u);
    CHECK(button1->has_state(broa11y::State::Focused));
    CHECK(!button2->has_state(broa11y::State::Focused));

    tree.set_focus(4);
    CHECK_EQ(tree.focused_node_id(), 4u);
    CHECK(!button1->has_state(broa11y::State::Focused));
    CHECK(button2->has_state(broa11y::State::Focused));

    tree.clear_focus();
    CHECK_EQ(tree.focused_node_id(), broa11y::kInvalidNodeId);
    CHECK(!button2->has_state(broa11y::State::Focused));

    // 8. Test Attributes & Relations
    button1->set_attribute("tooltip", "Submit form");
    auto tip = button1->get_attribute("tooltip");
    CHECK(tip.has_value());
    CHECK_EQ(*tip, "Submit form");

    button1->add_relation(broa11y::RelationType::ControllerFor, 2);
    auto rels = button1->get_relations(broa11y::RelationType::ControllerFor);
    CHECK_EQ(rels.size(), 1u);
    CHECK_EQ(rels[0], 2u);

    // 9. Test DFS traversal
    std::vector<broa11y::NodeId> dfs_order;
    tree.for_each_dfs([&](const broa11y::Node* n) {
        dfs_order.push_back(n->id());
        return true;
    });
    CHECK_EQ(dfs_order.size(), 4u);
    CHECK_EQ(dfs_order[0], 1u);
    CHECK_EQ(dfs_order[1], 2u);
    CHECK_EQ(dfs_order[2], 3u);
    CHECK_EQ(dfs_order[3], 4u);

    // 10. Test Removal
    tree.remove_node(2); // Should remove panel and both buttons
    CHECK_EQ(tree.node_count(), 1u);
    CHECK_EQ(tree.get_node(2), nullptr);
    CHECK_EQ(tree.get_node(3), nullptr);
    CHECK_EQ(tree.get_node(4), nullptr);
    CHECK_EQ(window->child_count(), 0u);

    return check::finish("test_tree");
}
