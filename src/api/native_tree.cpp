#include "host_a11y_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <cctype>
#include <string>
#include <vector>

namespace broa11y::api {

namespace {

std::string toSnakeCase(std::string_view str) {
    std::string res;
    res.reserve(str.size() + 4);
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') {
            if (i > 0 && str[i - 1] != '_') {
                res.push_back('_');
            }
            res.push_back(static_cast<char>(c + ('a' - 'A')));
        } else if (c == '-') {
            res.push_back('_');
        } else {
            res.push_back(c);
        }
    }
    return res;
}

} // namespace

Role parseRole(std::string_view str) {
    Role r = string_to_role(str);
    if (r != Role::Unknown) return r;

    std::string snake = toSnakeCase(str);
    r = string_to_role(snake);
    if (r != Role::Unknown) return r;

    if (str == "checkBox" || snake == "check_box") return Role::CheckBox;
    if (str == "toolBar" || snake == "tool_bar") return Role::ToolBar;
    if (str == "toolTip" || snake == "tool_tip") return Role::ToolTip;
    if (str == "comboBox" || snake == "combo_box") return Role::ComboBox;
    if (str == "scrollPane" || snake == "scroll_pane") return Role::ScrollPane;
    if (str == "progressBar" || snake == "progress_bar") return Role::ProgressBar;
    if (str == "scrollBar" || snake == "scroll_bar") return Role::ScrollBar;
    return Role::Unknown;
}

State parseState(std::string_view str) {
    State s = string_to_state(str);
    if (s != State::Count) return s;

    std::string snake = toSnakeCase(str);
    s = string_to_state(snake);
    if (s != State::Count) return s;

    if (str == "readOnly" || snake == "read_only") return State::ReadOnly;
    if (str == "multiSelectable" || snake == "multi_selectable") return State::MultiSelectable;
    if (str == "multiLine" || snake == "multi_line") return State::MultiLine;
    if (str == "singleLine" || snake == "single_line") return State::SingleLine;
    if (str == "hasPopup" || snake == "has_popup") return State::HasPopup;
    if (str == "selectableText" || snake == "selectable_text") return State::SelectableText;
    return State::Count;
}

std::string roleToString(Role role) {
    return std::string(role_to_string(role));
}

std::string stateToString(State state) {
    return std::string(state_to_string(state));
}

Value nodeToJs(const broa11y::Node* node) {
    if (!node) return ev::null();

    NodeId id = node->id();
    ObjectBuilder b;

    b.set("id", static_cast<double>(id));
    b.set("role", roleToString(node->role()));
    b.set("name", node->name());
    b.set("description", node->description());
    b.set("text", node->text());
    b.set("caretOffset", static_cast<double>(node->caret_offset()));
    b.set("caret_offset", static_cast<double>(node->caret_offset()));

    // bounds: { x, y, width, height }
    ObjectBuilder boundsB;
    boundsB.set("x", node->bounds().x);
    boundsB.set("y", node->bounds().y);
    boundsB.set("width", node->bounds().width);
    boundsB.set("height", node->bounds().height);
    b.set("bounds", boundsB.build());

    // parentId
    if (node->parent_id() == kInvalidNodeId) {
        b.set("parentId", ev::null());
        b.set("parent_id", ev::null());
    } else {
        b.set("parentId", static_cast<double>(node->parent_id()));
        b.set("parent_id", static_cast<double>(node->parent_id()));
    }

    // childIds and children
    const auto& cids = node->children_ids();
    ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(cids.size())));
    for (uint32_t i = 0; i < cids.size(); ++i) {
        ev::Persistent v(ev::fromDouble(static_cast<double>(cids[i])));
        arr.set(ev::setElement(arr.get(), i, v.get()));
    }
    b.set("childIds", arr.get());
    b.set("child_ids", arr.get());
    b.set("children", arr.get());

    // states
    auto stVec = node->states().to_vector();
    ev::Persistent stArr(ev::makeArray(static_cast<uint32_t>(stVec.size())));
    for (uint32_t i = 0; i < stVec.size(); ++i) {
        ev::Persistent v(ev::fromUtf8(stateToString(stVec[i])));
        stArr.set(ev::setElement(stArr.get(), i, v.get()));
    }
    b.set("states", stArr.get());

    // value
    if (node->value().has_value()) {
        ObjectBuilder vb;
        vb.set("current", node->value()->current);
        vb.set("minimum", node->value()->minimum);
        vb.set("maximum", node->value()->maximum);
        vb.set("step", node->value()->step);
        b.set("value", vb.build());
    } else {
        b.set("value", ev::null());
    }

    // selection
    ObjectBuilder sb;
    sb.set("startOffset", static_cast<double>(node->selection().start_offset));
    sb.set("endOffset", static_cast<double>(node->selection().end_offset));
    b.set("selection", sb.build());

    // attributes
    ObjectBuilder ab;
    for (const auto& [k, v] : node->attributes()) {
        ab.set(k, v);
    }
    b.set("attributes", ab.build());

    // actions
    const auto& acts = node->actions();
    ev::Persistent actArr(ev::makeArray(static_cast<uint32_t>(acts.size())));
    for (uint32_t i = 0; i < acts.size(); ++i) {
        ObjectBuilder actObj;
        actObj.set("name", acts[i].name);
        actObj.set("description", acts[i].description);
        actObj.set("keyBinding", acts[i].key_binding);
        actObj.set("key_binding", acts[i].key_binding);
        ev::Persistent itemP(actObj.build());
        actArr.set(ev::setElement(actArr.get(), i, itemP.get()));
    }
    b.set("actions", actArr.get());

    // Methods
    b.def("hasState", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0)) return ev::fromBool(false);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        State s = parseState(reader.getString(0));
        if (s == State::Count) return ev::fromBool(false);
        return ev::fromBool(n->has_state(s));
    });

    b.def("setName", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->set_name(reader.getString(0));
        return ev::fromBool(true);
    });

    b.def("setRole", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        Role r = parseRole(reader.getString(0));
        n->set_role(r);
        return ev::fromBool(true);
    });

    b.def("setDescription", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->set_description(reader.getString(0));
        return ev::fromBool(true);
    });

    b.def("setBounds", 4, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        if (reader.isObject(0)) {
            const auto& o = reader.getPersistent(0);
            RectF r{
                .x = ArgReader::getPropDouble(o, "x"),
                .y = ArgReader::getPropDouble(o, "y"),
                .width = ArgReader::getPropDouble(o, "width", ArgReader::getPropDouble(o, "w")),
                .height = ArgReader::getPropDouble(o, "height", ArgReader::getPropDouble(o, "h"))
            };
            n->set_bounds(r);
            return ev::fromBool(true);
        }
        RectF r{
            .x = reader.getDouble(0),
            .y = reader.getDouble(1),
            .width = reader.getDouble(2),
            .height = reader.getDouble(3)
        };
        n->set_bounds(r);
        return ev::fromBool(true);
    });

    b.def("setState", 2, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        State s = parseState(reader.getString(0));
        if (s == State::Count) return ev::fromBool(false);
        bool val = reader.has(1) ? reader.getBool(1) : true;
        n->set_state(s, val);
        return ev::fromBool(true);
    });

    b.def("setValue", 4, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        if (reader.isObject(0)) {
            const auto& o = reader.getPersistent(0);
            ValueRange vr{
                .current = ArgReader::getPropDouble(o, "current"),
                .minimum = ArgReader::getPropDouble(o, "minimum", ArgReader::getPropDouble(o, "min", 0.0)),
                .maximum = ArgReader::getPropDouble(o, "maximum", ArgReader::getPropDouble(o, "max", 100.0)),
                .step = ArgReader::getPropDouble(o, "step", 1.0)
            };
            n->set_value(vr);
            return ev::fromBool(true);
        }
        ValueRange vr{
            .current = reader.getDouble(0),
            .minimum = reader.has(1) ? reader.getDouble(1) : 0.0,
            .maximum = reader.has(2) ? reader.getDouble(2) : 100.0,
            .step = reader.has(3) ? reader.getDouble(3) : 1.0
        };
        n->set_value(vr);
        return ev::fromBool(true);
    });

    b.def("clearValue", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->clear_value();
        return ev::fromBool(true);
    });

    b.def("setText", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->set_text(reader.getString(0));
        return ev::fromBool(true);
    });

    b.def("setCaretOffset", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->set_caret_offset(reader.getInt(0));
        return ev::fromBool(true);
    });

    b.def("setSelection", 2, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        if (reader.isObject(0)) {
            const auto& o = reader.getPersistent(0);
            TextRange tr{
                .start_offset = ArgReader::getPropInt(o, "startOffset", ArgReader::getPropInt(o, "start")),
                .end_offset = ArgReader::getPropInt(o, "endOffset", ArgReader::getPropInt(o, "end"))
            };
            n->set_selection(tr);
            return ev::fromBool(true);
        }
        TextRange tr{
            .start_offset = reader.getInt(0),
            .end_offset = reader.getInt(1)
        };
        n->set_selection(tr);
        return ev::fromBool(true);
    });

    b.def("setAttribute", 2, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        n->set_attribute(reader.getString(0), reader.getString(1));
        return ev::fromBool(true);
    });

    b.def("getAttribute", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::null();
        auto n = tree->get_node(id);
        if (!n) return ev::null();
        auto attr = n->get_attribute(reader.getString(0));
        return attr ? ev::fromUtf8(*attr) : ev::null();
    });

    b.def("addAction", 3, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        if (reader.isObject(0)) {
            const auto& o = reader.getPersistent(0);
            ActionDescriptor ad{
                .name = ArgReader::getPropString(o, "name"),
                .description = ArgReader::getPropString(o, "description"),
                .key_binding = ArgReader::getPropString(o, "keyBinding", ArgReader::getPropString(o, "key_binding"))
            };
            n->add_action(std::move(ad));
            return ev::fromBool(true);
        }
        ActionDescriptor ad{
            .name = reader.getString(0),
            .description = reader.getString(1),
            .key_binding = reader.getString(2)
        };
        n->add_action(std::move(ad));
        return ev::fromBool(true);
    });

    b.def("focus", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        return ev::fromBool(tree->set_focus(id));
    });

    b.def("performAction", 2, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        std::string act = reader.getString(0);
        ActionParams params;
        if (reader.isObject(1)) {
            const auto& p = reader.getPersistent(1);
            std::string strVal = ArgReader::getPropString(p, "stringVal");
            if (strVal.empty()) strVal = ArgReader::getPropString(p, "value");
            params.string_val = std::move(strVal);

            double numVal = ArgReader::getPropDouble(p, "numberVal");
            if (numVal == 0.0 && ArgReader::hasProp(p, "value")) {
                numVal = ArgReader::getPropDouble(p, "value");
            }
            params.number_val = numVal;
            params.int_val = ArgReader::getPropInt(p, "intVal");
            if (ArgReader::hasProp(p, "pointVal")) {
                ev::Persistent pt(ArgReader::getProp(p, "pointVal"));
                params.point_val.x = ArgReader::getPropDouble(pt, "x");
                params.point_val.y = ArgReader::getPropDouble(pt, "y");
            }
            if (ArgReader::hasProp(p, "rangeVal")) {
                ev::Persistent r(ArgReader::getProp(p, "rangeVal"));
                params.range_val.start_offset = ArgReader::getPropInt(r, "startOffset");
                params.range_val.end_offset = ArgReader::getPropInt(r, "endOffset");
            }
        }
        return ev::fromBool(n->perform_action(act, params));
    });

    b.def("click", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        return ev::fromBool(n->perform_action(kActionActivate));
    });

    b.def("activate", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);
        return ev::fromBool(n->perform_action(kActionActivate));
    });

    b.def("onAction", 1, [id](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isFunction(0)) return ev::fromBool(false);
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);

        auto cb = std::make_shared<ev::Persistent>(reader.get(0));
        n->set_action_handler([cb](NodeId nid, std::string_view act, const ActionParams& params) -> bool {
            if (!cb || ev::isUndefined(cb->get()) || !ev::isFunction(cb->get())) return false;
            ObjectBuilder pb;
            pb.set("stringVal", params.string_val);
            pb.set("numberVal", params.number_val);
            pb.set("intVal", static_cast<double>(params.int_val));
            ObjectBuilder pt;
            pt.set("x", params.point_val.x);
            pt.set("y", params.point_val.y);
            pb.set("pointVal", pt.build());
            ObjectBuilder tr;
            tr.set("startOffset", static_cast<double>(params.range_val.start_offset));
            tr.set("endOffset", static_cast<double>(params.range_val.end_offset));
            pb.set("rangeVal", tr.build());

            ev::Persistent actP(ev::fromUtf8(act));
            ev::Persistent pObj(pb.build());
            ev::Persistent idVal(ev::fromDouble(static_cast<double>(nid)));
            const Value callArgs[3] = { actP.get(), pObj.get(), idVal.get() };
            auto res = ev::call(cb->get(), ev::undefined(), std::span<const Value>(callArgs, 3));
            if (res.thrown) return false;
            return ev::toBool(res.value);
        });
        return ev::fromBool(true);
    });

    b.def("setActionHandler", 1, [id](Value thisVal, std::span<const Value> args) -> Value {
        Value onFn = ev::getProperty(thisVal, "onAction");
        if (ev::isFunction(onFn)) {
            auto res = ev::call(onFn, thisVal, args);
            return res.thrown ? ev::fromBool(false) : res.value;
        }
        return ev::fromBool(false);
    });

    b.def("getParent", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::null();
        auto n = tree->get_node(id);
        if (!n || !n->parent()) return ev::null();
        return nodeToJs(n->parent());
    });

    b.def("getChildren", 0, [id](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::makeArray(0);
        auto n = tree->get_node(id);
        if (!n) return ev::makeArray(0);
        const auto& cids2 = n->children_ids();
        ev::Persistent resArr(ev::makeArray(static_cast<uint32_t>(cids2.size())));
        for (uint32_t i = 0; i < cids2.size(); ++i) {
            ev::Persistent childObj(nodeToJs(tree->get_node(cids2[i])));
            resArr.set(ev::setElement(resArr.get(), i, childObj.get()));
        }
        return resArr.get();
    });

    return b.build();
}

void installTreeOnto(Value a11yObj) {
    ObjectBuilder a11y(a11yObj);

    // bro.a11y.getRootNode() -> Object | null
    a11y.def("getRootNode", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree || !tree->root()) return ev::null();
        return nodeToJs(tree->root());
    });

    // bro.a11y.getNode(id) -> Object | null
    a11y.def("getNode", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isNumber(0)) return ev::null();
        auto tree = activeTree();
        if (!tree) return ev::null();
        uint64_t id = reader.getUint64(0);
        auto n = tree->get_node(id);
        return n ? nodeToJs(n) : ev::null();
    });

    // bro.a11y.createNode(roleOrOptions?, id?) -> Object
    a11y.def("createNode", 2, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::null();

        Role role = Role::Unknown;
        NodeId id = kInvalidNodeId;
        std::string name;
        std::string desc;
        std::string text;
        bool hasParent = false;
        NodeId parentId = kInvalidNodeId;
        bool hasBounds = false;
        RectF bounds{};

        if (reader.isString(0)) {
            role = parseRole(reader.getString(0));
            if (reader.isNumber(1)) {
                id = reader.getUint64(1);
            }
        } else if (reader.isObject(0)) {
            const auto& opt = reader.getPersistent(0);
            if (ArgReader::hasProp(opt, "role")) {
                role = parseRole(ArgReader::getPropString(opt, "role"));
            }
            if (ArgReader::hasProp(opt, "id")) {
                id = ArgReader::getPropUint64(opt, "id");
            } else if (reader.isNumber(1)) {
                id = reader.getUint64(1);
            }
            name = ArgReader::getPropString(opt, "name");
            desc = ArgReader::getPropString(opt, "description");
            text = ArgReader::getPropString(opt, "text");
            if (ArgReader::hasProp(opt, "parentId")) {
                hasParent = true;
                parentId = ArgReader::getPropUint64(opt, "parentId");
            } else if (ArgReader::hasProp(opt, "parent_id")) {
                hasParent = true;
                parentId = ArgReader::getPropUint64(opt, "parent_id");
            }
            if (ArgReader::hasProp(opt, "bounds")) {
                ev::Persistent b(ArgReader::getProp(opt, "bounds"));
                hasBounds = true;
                bounds.x = ArgReader::getPropDouble(b, "x");
                bounds.y = ArgReader::getPropDouble(b, "y");
                bounds.width = ArgReader::getPropDouble(b, "width");
                bounds.height = ArgReader::getPropDouble(b, "height");
            }
        } else if (reader.isNumber(0)) {
            id = reader.getUint64(0);
        }

        Node* n = (id == kInvalidNodeId) ? tree->create_node_with_role(role)
                                         : tree->create_node_with_role(role, id);
        if (!n) return ev::null();

        if (!name.empty()) n->set_name(name);
        if (!desc.empty()) n->set_description(desc);
        if (!text.empty()) n->set_text(text);
        if (hasBounds) n->set_bounds(bounds);

        if (tree->root_id() == kInvalidNodeId) {
            tree->set_root_id(n->id());
        }

        if (hasParent && parentId != kInvalidNodeId) {
            tree->reparent_node(n->id(), parentId);
        }

        return nodeToJs(n);
    });

    // bro.a11y.removeNode(id) -> boolean
    a11y.def("removeNode", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isNumber(0)) return ev::fromBool(false);
        return ev::fromBool(tree->remove_node(reader.getUint64(0)));
    });

    // bro.a11y.reparent(childId, parentId, index?) -> boolean
    a11y.def("reparent", 3, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isNumber(0) || !reader.isNumber(1)) return ev::fromBool(false);
        size_t idx = reader.has(2) ? static_cast<size_t>(reader.getUint(2)) : static_cast<size_t>(-1);
        return ev::fromBool(tree->reparent_node(reader.getUint64(0), reader.getUint64(1), idx));
    });

    // bro.a11y.setFocus(id) -> boolean
    a11y.def("setFocus", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isNumber(0)) return ev::fromBool(false);
        return ev::fromBool(tree->set_focus(reader.getUint64(0)));
    });

    // bro.a11y.clearFocus() -> boolean
    a11y.def("clearFocus", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        tree->clear_focus();
        return ev::fromBool(true);
    });

    // bro.a11y.getFocusedNode() -> Object | null
    a11y.def("getFocusedNode", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::null();
        auto n = tree->focused_node();
        return n ? nodeToJs(n) : ev::null();
    });

    // bro.a11y.hitTest(x, y) -> Object | null
    a11y.def("hitTest", 2, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree) return ev::null();
        PointF pt{ reader.getDouble(0), reader.getDouble(1) };
        NodeId hitId = tree->hit_test(pt);
        if (hitId == kInvalidNodeId) return ev::null();
        auto n = tree->get_node(hitId);
        return n ? nodeToJs(n) : ev::null();
    });

    // bro.a11y.findNodesByRole(role) -> Array
    a11y.def("findNodesByRole", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isString(0)) return ev::makeArray(0);
        Role r = parseRole(reader.getString(0));
        auto found = tree->find_by_role(r);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(found.size())));
        for (uint32_t i = 0; i < found.size(); ++i) {
            ev::Persistent item(nodeToJs(found[i]));
            arr.set(ev::setElement(arr.get(), i, item.get()));
        }
        return arr.get();
    });

    // bro.a11y.findNodesByName(name) -> Array
    a11y.def("findNodesByName", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isString(0)) return ev::makeArray(0);
        auto found = tree->find_by_name(reader.getString(0));
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(found.size())));
        for (uint32_t i = 0; i < found.size(); ++i) {
            ev::Persistent item(nodeToJs(found[i]));
            arr.set(ev::setElement(arr.get(), i, item.get()));
        }
        return arr.get();
    });

    // bro.a11y.beginTransaction() -> boolean
    a11y.def("beginTransaction", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        tree->begin_transaction();
        return ev::fromBool(true);
    });

    // bro.a11y.commitTransaction() -> boolean
    a11y.def("commitTransaction", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        tree->commit_transaction();
        return ev::fromBool(true);
    });

    // bro.a11y.rollbackTransaction() -> boolean
    a11y.def("rollbackTransaction", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        tree->rollback_transaction();
        return ev::fromBool(true);
    });

    // bro.a11y.inTransaction() -> boolean
    a11y.def("inTransaction", 0, [](Value, std::span<const Value>) -> Value {
        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        return ev::fromBool(tree->in_transaction());
    });

    // bro.a11y.performAction(nodeId, actionName, params?) -> boolean
    a11y.def("performAction", 3, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        auto tree = activeTree();
        if (!tree || !reader.isNumber(0) || !reader.isString(1)) return ev::fromBool(false);
        uint64_t id = reader.getUint64(0);
        auto n = tree->get_node(id);
        if (!n) return ev::fromBool(false);

        std::string act = reader.getString(1);
        ActionParams params;
        if (reader.isObject(2)) {
            const auto& p = reader.getPersistent(2);
            std::string strVal = ArgReader::getPropString(p, "stringVal");
            if (strVal.empty()) strVal = ArgReader::getPropString(p, "value");
            params.string_val = std::move(strVal);

            double numVal = ArgReader::getPropDouble(p, "numberVal");
            if (numVal == 0.0 && ArgReader::hasProp(p, "value")) {
                numVal = ArgReader::getPropDouble(p, "value");
            }
            params.number_val = numVal;
            params.int_val = ArgReader::getPropInt(p, "intVal");
            if (ArgReader::hasProp(p, "pointVal")) {
                ev::Persistent pt(ArgReader::getProp(p, "pointVal"));
                params.point_val.x = ArgReader::getPropDouble(pt, "x");
                params.point_val.y = ArgReader::getPropDouble(pt, "y");
            }
            if (ArgReader::hasProp(p, "rangeVal")) {
                ev::Persistent r(ArgReader::getProp(p, "rangeVal"));
                params.range_val.start_offset = ArgReader::getPropInt(r, "startOffset");
                params.range_val.end_offset = ArgReader::getPropInt(r, "endOffset");
            }
        }
        return ev::fromBool(n->perform_action(act, params));
    });
}

} // namespace broa11y::api
