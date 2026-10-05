#pragma once

#include "broa11y/action.h"
#include "broa11y/role.h"
#include "broa11y/state.h"
#include "broa11y/types.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace broa11y {

class Tree;

struct NodeData {
    NodeId id = kInvalidNodeId;
    Role role = Role::Unknown;
    StateSet states{};
    RectF bounds{};
    std::string name;
    std::string description;
    std::optional<ValueRange> value;
    std::string text;
    int32_t caret_offset = -1;
    TextRange selection{};
    std::unordered_map<std::string, std::string> attributes;
    std::vector<ActionDescriptor> actions;
    std::unordered_map<uint32_t, std::vector<NodeId>> relations;
};

class Node {
public:
    explicit Node(NodeId id = kInvalidNodeId, Role role = Role::Unknown);
    explicit Node(NodeData data);
    ~Node() = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;

    [[nodiscard]] NodeId id() const noexcept { return data_.id; }
    void set_id(NodeId id) noexcept { data_.id = id; }

    [[nodiscard]] Role role() const noexcept { return data_.role; }
    void set_role(Role role);

    [[nodiscard]] const StateSet& states() const noexcept { return data_.states; }
    [[nodiscard]] StateSet& states() noexcept { return data_.states; }
    [[nodiscard]] bool has_state(State s) const noexcept { return data_.states.has(s); }
    void set_state(State s, bool value = true);

    [[nodiscard]] const RectF& bounds() const noexcept { return data_.bounds; }
    void set_bounds(const RectF& bounds);

    [[nodiscard]] const std::string& name() const noexcept { return data_.name; }
    void set_name(std::string_view name);

    [[nodiscard]] const std::string& description() const noexcept { return data_.description; }
    void set_description(std::string_view desc);

    [[nodiscard]] const std::optional<ValueRange>& value() const noexcept { return data_.value; }
    void set_value(const ValueRange& value);
    void clear_value();

    [[nodiscard]] const std::string& text() const noexcept { return data_.text; }
    void set_text(std::string_view text);

    [[nodiscard]] int32_t caret_offset() const noexcept { return data_.caret_offset; }
    void set_caret_offset(int32_t offset);

    [[nodiscard]] const TextRange& selection() const noexcept { return data_.selection; }
    void set_selection(const TextRange& selection);

    [[nodiscard]] const std::unordered_map<std::string, std::string>& attributes() const noexcept {
        return data_.attributes;
    }
    void set_attribute(std::string_view key, std::string_view value);
    [[nodiscard]] std::optional<std::string> get_attribute(std::string_view key) const;

    void add_relation(RelationType type, NodeId target_id);
    void remove_relation(RelationType type, NodeId target_id);
    [[nodiscard]] std::vector<NodeId> get_relations(RelationType type) const;

    void add_action(ActionDescriptor action);
    [[nodiscard]] const std::vector<ActionDescriptor>& actions() const noexcept { return data_.actions; }
    void set_action_handler(ActionHandler handler) { action_handler_ = std::move(handler); }
    bool perform_action(std::string_view action_name, const ActionParams& params = {});

    [[nodiscard]] NodeId parent_id() const noexcept { return parent_id_; }
    [[nodiscard]] const std::vector<NodeId>& children_ids() const noexcept { return children_ids_; }
    [[nodiscard]] size_t child_count() const noexcept { return children_ids_.size(); }
    [[nodiscard]] NodeId child_at(size_t index) const noexcept;
    [[nodiscard]] int32_t index_in_parent() const noexcept;

    // Tree navigation
    [[nodiscard]] Node* parent() const noexcept;
    [[nodiscard]] Node* first_child() const noexcept;
    [[nodiscard]] Node* last_child() const noexcept;
    [[nodiscard]] Node* next_sibling() const noexcept;
    [[nodiscard]] Node* previous_sibling() const noexcept;
    [[nodiscard]] Node* child_node_at(size_t index) const noexcept;

    [[nodiscard]] Tree* tree() const noexcept { return tree_; }

    [[nodiscard]] const NodeData& data() const noexcept { return data_; }

private:
    friend class Tree;

    void set_tree(Tree* tree) noexcept { tree_ = tree; }
    void set_parent_id(NodeId pid) noexcept { parent_id_ = pid; }
    void add_child_id(NodeId cid, size_t index = static_cast<size_t>(-1));
    bool remove_child_id(NodeId cid);

    NodeData data_;
    NodeId parent_id_ = kInvalidNodeId;
    std::vector<NodeId> children_ids_;
    Tree* tree_ = nullptr;
    ActionHandler action_handler_;
};

} // namespace broa11y
