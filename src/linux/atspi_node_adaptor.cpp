#include "atspi_node_adaptor.h"
#include "atspi_constants.h"
#include "atspi_serializer.h"
#include "broa11y/role.h"

#include <charconv>
#include <string>

namespace broa11y {
// Forward declaration of boundary calculation from terminal_navigation.cpp
void find_char_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end);
void find_word_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end);
void find_line_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end);
}

namespace broa11y::atspi {

std::string NodeAdaptor::node_id_to_path(NodeId id, NodeId root_id) {
    if (id == root_id && root_id != kInvalidNodeId) {
        return std::string(kPathRoot);
    }
    return std::string(kPathPrefix) + std::to_string(id);
}

NodeId NodeAdaptor::path_to_node_id(std::string_view path, NodeId root_id) {
    if (path == kPathRoot) {
        return root_id;
    }
    if (path.starts_with(kPathPrefix)) {
        auto num_part = path.substr(kPathPrefix.size());
        NodeId val = 0;
        auto [ptr, ec] = std::from_chars(num_part.data(), num_part.data() + num_part.size(), val);
        if (ec == std::errc()) {
            return val;
        }
    }
    return kInvalidNodeId;
}

Reference NodeAdaptor::make_reference(std::string_view bus_name, NodeId id, NodeId root_id) {
    return Reference{
        .sender = std::string(bus_name),
        .path = node_id_to_path(id, root_id)
    };
}

MethodReply NodeAdaptor::handle_call(Tree* tree, std::string_view bus_name, const MethodCall& call) {
    if (!tree) {
        return MethodReply{.success = false, .error_message = "No tree available"};
    }

    NodeId nid = path_to_node_id(call.path, tree->root_id());
    Node* node = tree->get_node(nid);
    if (!node) {
        return MethodReply{.success = false, .error_message = "Unknown object path: " + call.path};
    }

    if (call.interface_name == kDbusInterfaceAccessible) {
        return handle_accessible(node, tree, bus_name, call.member, call.args);
    } else if (call.interface_name == kDbusInterfaceComponent) {
        return handle_component(node, tree, bus_name, call.member, call.args);
    } else if (call.interface_name == kDbusInterfaceAction) {
        return handle_action(node, call.member, call.args);
    } else if (call.interface_name == kDbusInterfaceText) {
        return handle_text(node, call.member, call.args);
    } else if (call.interface_name == kDbusInterfaceEditableText) {
        return handle_editable_text(node, call.member, call.args);
    } else if (call.interface_name == kDbusInterfaceValue) {
        return handle_value(node, call.member, call.args);
    }

    return MethodReply{.success = false, .error_message = "Unsupported interface: " + call.interface_name};
}

MethodReply NodeAdaptor::handle_accessible(const Node* node,
                                          Tree* tree,
                                          std::string_view bus_name,
                                          std::string_view member,
                                          const std::vector<std::string>& args) {
    if (member == "GetRole") {
        uint32_t atspi_role = role_to_atspi_role(node->role());
        return MethodReply{.signature = "u", .values = {Serializer::encode_uint32(atspi_role)}};
    }
    if (member == "GetRoleName") {
        return MethodReply{.signature = "s", .values = {Serializer::encode_string(role_to_atspi_name(node->role()))}};
    }
    if (member == "GetState") {
        auto [low, high] = node->states().to_atspi_state_bitmask();
        return MethodReply{.signature = "au", .values = {Serializer::encode_state_set(low, high)}};
    }
    if (member == "GetAttributes") {
        return MethodReply{.signature = "a{ss}", .values = {Serializer::encode_string_map(node->attributes())}};
    }
    if (member == "GetApplication") {
        Reference app_ref = make_reference(bus_name, tree->root_id(), tree->root_id());
        return MethodReply{.signature = "(so)", .values = {Serializer::encode_reference(app_ref)}};
    }
    if (member == "GetChildAtIndex") {
        int idx = !args.empty() ? std::stoi(args[0]) : -1;
        if (idx >= 0 && static_cast<size_t>(idx) < node->child_count()) {
            NodeId cid = node->child_at(static_cast<size_t>(idx));
            return MethodReply{.signature = "(so)", .values = {Serializer::encode_reference(make_reference(bus_name, cid, tree->root_id()))}};
        }
        return MethodReply{.success = false, .error_message = "Index out of bounds"};
    }
    if (member == "GetChildren") {
        std::vector<Reference> refs;
        for (NodeId cid : node->children_ids()) {
            refs.push_back(make_reference(bus_name, cid, tree->root_id()));
        }
        return MethodReply{.signature = "a(so)", .values = {Serializer::encode_reference_list(refs)}};
    }
    if (member == "GetIndexInParent") {
        return MethodReply{.signature = "i", .values = {Serializer::encode_int32(node->index_in_parent())}};
    }
    if (member == "Name") {
        return MethodReply{.signature = "s", .values = {Serializer::encode_string(node->name())}};
    }
    if (member == "Description") {
        return MethodReply{.signature = "s", .values = {Serializer::encode_string(node->description())}};
    }

    return MethodReply{.success = false, .error_message = "Unknown Accessible member: " + std::string(member)};
}

MethodReply NodeAdaptor::handle_component(Node* node,
                                         Tree* tree,
                                         std::string_view bus_name,
                                         std::string_view member,
                                         const std::vector<std::string>& args) {
    const RectF& b = node->bounds();
    if (member == "GetExtents") {
        Rect r{
            .x = static_cast<int32_t>(b.x),
            .y = static_cast<int32_t>(b.y),
            .width = static_cast<int32_t>(b.width),
            .height = static_cast<int32_t>(b.height)
        };
        return MethodReply{.signature = "(iiii)", .values = {Serializer::encode_rect(r)}};
    }
    if (member == "GetPosition") {
        Point pt{.x = static_cast<int32_t>(b.x), .y = static_cast<int32_t>(b.y)};
        return MethodReply{.signature = "(ii)", .values = {Serializer::encode_point(pt)}};
    }
    if (member == "GetSize") {
        Point pt{.x = static_cast<int32_t>(b.width), .y = static_cast<int32_t>(b.height)};
        return MethodReply{.signature = "(ii)", .values = {Serializer::encode_point(pt)}};
    }
    if (member == "Contains") {
        int x = args.size() > 0 ? std::stoi(args[0]) : 0;
        int y = args.size() > 1 ? std::stoi(args[1]) : 0;
        bool inside = b.contains({static_cast<double>(x), static_cast<double>(y)});
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(inside)}};
    }
    if (member == "GetAccessibleAtPoint") {
        int x = args.size() > 0 ? std::stoi(args[0]) : 0;
        int y = args.size() > 1 ? std::stoi(args[1]) : 0;
        NodeId hit = tree->hit_test_from(node->id(), {static_cast<double>(x), static_cast<double>(y)});
        return MethodReply{.signature = "(so)", .values = {Serializer::encode_reference(make_reference(bus_name, hit, tree->root_id()))}};
    }
    if (member == "GrabFocus") {
        bool ok = tree->set_focus(node->id());
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(ok)}};
    }

    return MethodReply{.success = false, .error_message = "Unknown Component member: " + std::string(member)};
}

MethodReply NodeAdaptor::handle_action(Node* node,
                                      std::string_view member,
                                      const std::vector<std::string>& args) {
    if (member == "GetNActions") {
        return MethodReply{.signature = "i", .values = {Serializer::encode_int32(static_cast<int32_t>(node->actions().size()))}};
    }
    if (member == "GetName") {
        int idx = !args.empty() ? std::stoi(args[0]) : -1;
        if (idx >= 0 && static_cast<size_t>(idx) < node->actions().size()) {
            return MethodReply{.signature = "s", .values = {Serializer::encode_string(node->actions()[static_cast<size_t>(idx)].name)}};
        }
        return MethodReply{.success = false, .error_message = "Index out of bounds"};
    }
    if (member == "GetDescription") {
        int idx = !args.empty() ? std::stoi(args[0]) : -1;
        if (idx >= 0 && static_cast<size_t>(idx) < node->actions().size()) {
            return MethodReply{.signature = "s", .values = {Serializer::encode_string(node->actions()[static_cast<size_t>(idx)].description)}};
        }
        return MethodReply{.success = false, .error_message = "Index out of bounds"};
    }
    if (member == "GetKeyBinding") {
        int idx = !args.empty() ? std::stoi(args[0]) : -1;
        if (idx >= 0 && static_cast<size_t>(idx) < node->actions().size()) {
            return MethodReply{.signature = "s", .values = {Serializer::encode_string(node->actions()[static_cast<size_t>(idx)].key_binding)}};
        }
        return MethodReply{.success = false, .error_message = "Index out of bounds"};
    }
    if (member == "DoAction") {
        int idx = !args.empty() ? std::stoi(args[0]) : -1;
        if (idx >= 0 && static_cast<size_t>(idx) < node->actions().size()) {
            bool ok = node->perform_action(node->actions()[static_cast<size_t>(idx)].name);
            return MethodReply{.signature = "b", .values = {Serializer::encode_bool(ok)}};
        }
        return MethodReply{.success = false, .error_message = "Index out of bounds"};
    }

    return MethodReply{.success = false, .error_message = "Unknown Action member: " + std::string(member)};
}

MethodReply NodeAdaptor::handle_text(Node* node,
                                    std::string_view member,
                                    const std::vector<std::string>& args) {
    const std::string& txt = node->text();
    if (member == "GetCharacterCount") {
        return MethodReply{.signature = "i", .values = {Serializer::encode_int32(static_cast<int32_t>(txt.size()))}};
    }
    if (member == "GetCaretOffset") {
        return MethodReply{.signature = "i", .values = {Serializer::encode_int32(node->caret_offset())}};
    }
    if (member == "SetCaretOffset") {
        int off = !args.empty() ? std::stoi(args[0]) : 0;
        node->set_caret_offset(off);
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }
    if (member == "GetText") {
        int start = args.size() > 0 ? std::stoi(args[0]) : 0;
        int end = args.size() > 1 ? std::stoi(args[1]) : static_cast<int>(txt.size());
        if (start < 0) start = 0;
        if (end > static_cast<int>(txt.size())) end = static_cast<int>(txt.size());
        std::string sub = (end > start) ? txt.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)) : "";
        return MethodReply{.signature = "s", .values = {Serializer::encode_string(sub)}};
    }
    if (member == "GetTextAtOffset") {
        int off = args.size() > 0 ? std::stoi(args[0]) : 0;
        uint32_t boundary = args.size() > 1 ? static_cast<uint32_t>(std::stoul(args[1])) : 0;
        int32_t start = 0, end = 0;
        if (boundary == 0) { // Char
            find_char_bounds(txt, off, &start, &end);
        } else if (boundary == 1 || boundary == 2) { // Word
            find_word_bounds(txt, off, &start, &end);
        } else { // Line
            find_line_bounds(txt, off, &start, &end);
        }
        std::string slice_text = (end > start && start < static_cast<int32_t>(txt.size()))
            ? txt.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)) : "";
        return MethodReply{.signature = "(sii)", .values = {Serializer::encode_text_slice({slice_text, start, end})}};
    }
    if (member == "GetNSelections") {
        int n = node->selection().is_empty() ? 0 : 1;
        return MethodReply{.signature = "i", .values = {Serializer::encode_int32(n)}};
    }
    if (member == "GetSelection") {
        const auto& sel = node->selection();
        return MethodReply{.signature = "(ii)", .values = {Serializer::encode_point({sel.start_offset, sel.end_offset})}};
    }
    if (member == "SetSelection") {
        int s = args.size() > 1 ? std::stoi(args[1]) : 0;
        int e = args.size() > 2 ? std::stoi(args[2]) : 0;
        node->set_selection(TextRange{s, e});
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }

    return MethodReply{.success = false, .error_message = "Unknown Text member: " + std::string(member)};
}

MethodReply NodeAdaptor::handle_editable_text(Node* node,
                                             std::string_view member,
                                             const std::vector<std::string>& args) {
    if (member == "SetTextContents") {
        std::string content = !args.empty() ? args[0] : "";
        node->set_text(content);
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }
    if (member == "InsertText") {
        int pos = args.size() > 0 ? std::stoi(args[0]) : 0;
        std::string to_ins = args.size() > 1 ? args[1] : "";
        std::string cur = node->text();
        if (pos < 0) pos = 0;
        if (pos > static_cast<int>(cur.size())) pos = static_cast<int>(cur.size());
        cur.insert(static_cast<size_t>(pos), to_ins);
        node->set_text(cur);
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }
    if (member == "DeleteText") {
        int start = args.size() > 0 ? std::stoi(args[0]) : 0;
        int end = args.size() > 1 ? std::stoi(args[1]) : 0;
        std::string cur = node->text();
        if (start < 0) start = 0;
        if (end > static_cast<int>(cur.size())) end = static_cast<int>(cur.size());
        if (end > start) {
            cur.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
            node->set_text(cur);
        }
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }

    return MethodReply{.success = false, .error_message = "Unknown EditableText member: " + std::string(member)};
}

MethodReply NodeAdaptor::handle_value(Node* node,
                                     std::string_view member,
                                     const std::vector<std::string>& args) {
    const auto& v = node->value().value_or(ValueRange{});
    if (member == "MinimumValue") {
        return MethodReply{.signature = "d", .values = {Serializer::encode_double(v.minimum)}};
    }
    if (member == "MaximumValue") {
        return MethodReply{.signature = "d", .values = {Serializer::encode_double(v.maximum)}};
    }
    if (member == "CurrentValue") {
        return MethodReply{.signature = "d", .values = {Serializer::encode_double(v.current)}};
    }
    if (member == "MinimumIncrement") {
        return MethodReply{.signature = "d", .values = {Serializer::encode_double(v.step)}};
    }
    if (member == "SetCurrentValue") {
        double new_val = !args.empty() ? std::stod(args[0]) : 0.0;
        ValueRange nv = v;
        nv.current = new_val;
        node->set_value(nv);
        return MethodReply{.signature = "b", .values = {Serializer::encode_bool(true)}};
    }

    return MethodReply{.success = false, .error_message = "Unknown Value member: " + std::string(member)};
}

} // namespace broa11y::atspi
