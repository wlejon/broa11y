// The AT-SPI object interfaces: Accessible, Application, Component, Action,
// Text, EditableText, Value, and the Cache. Every node shares one fallback
// vtable per interface under /org/a11y/atspi/accessible; find() admits a path
// only when it names a live node that has that interface.
//
// Requests that change things (DoAction, CurrentValue, SetCaretOffset,
// SetTextContents, GrabFocus, ...) go to the node's ActionHandler; the
// application decides and updates the tree, and the tree's events become the
// AT-SPI signals the client sees.
#include "linux/atspi_util.h"
#include "broa11y/version.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace broa11y::atspi {

namespace {

bool perform(Node* n, std::string_view action, const ActionParams& p = {}) {
    return n->perform_action(action, p);
}

// ── org.a11y.atspi.Accessible ────────────────────────────────────────────────

int prop_name(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
              sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path, t, err)) return -ENOENT;
    if (t.app) return sd_bus_message_append(reply, "s", s->config.app_name.c_str());
    return sd_bus_message_append(reply, "s", s->tree->get_node(t.id)->name().c_str());
}

int prop_description(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                     sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path, t, err)) return -ENOENT;
    return sd_bus_message_append(reply, "s", t.app ? "" : s->tree->get_node(t.id)->description().c_str());
}

int append_parent(Server* s, const Target& t, sd_bus_message* m) {
    if (t.app) {
        return sd_bus_message_append(m, "(so)", s->desktop_bus.c_str(), s->desktop_path.c_str());
    }
    if (t.id == s->tree->root_id()) return s->append_app_ref(m);
    return s->append_ref(m, s->tree->get_node(t.id)->parent_id());
}

int prop_parent(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path, t, err)) return -ENOENT;
    return append_parent(s, t, reply);
}

int32_t child_count(Server* s, const Target& t) {
    if (t.app) return s->tree->root() ? 1 : 0;
    return static_cast<int32_t>(s->tree->get_node(t.id)->child_count());
}

int prop_child_count(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                     sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path, t, err)) return -ENOENT;
    return sd_bus_message_append(reply, "i", child_count(s, t));
}

int prop_locale(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "");
}

int prop_accessible_id(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                       sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path, t, err)) return -ENOENT;
    return sd_bus_message_append(reply, "s", t.app ? "" : std::to_string(t.id).c_str());
}

int m_get_child_at_index(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    int32_t index = 0;
    int r = sd_bus_message_read(m, "i", &index);
    if (r < 0) return r;
    Reply rep(m);
    if (t.app) {
        rep.r = index == 0 ? s->append_ref(rep.msg, s->tree->root_id()) : s->append_ref(rep.msg, kInvalidNodeId);
    } else {
        const Node* n = s->tree->get_node(t.id);
        rep.r = s->append_ref(rep.msg, index >= 0 ? n->child_at(static_cast<size_t>(index)) : kInvalidNodeId);
    }
    return rep.send();
}

int m_get_children(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_open_container(rep.msg, 'a', "(so)");
    if (t.app) {
        if (s->tree->root()) s->append_ref(rep.msg, s->tree->root_id());
    } else {
        for (NodeId c : s->tree->get_node(t.id)->children_ids()) s->append_ref(rep.msg, c);
    }
    rep.r = sd_bus_message_close_container(rep.msg);
    return rep.send();
}

int m_get_index_in_parent(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    int32_t idx = -1;
    if (!t.app) idx = t.id == s->tree->root_id() ? 0 : s->tree->get_node(t.id)->index_in_parent();
    return sd_bus_reply_method_return(m, "i", idx);
}

uint32_t relation_code(RelationType r) {
    switch (r) {  // AtspiRelationType
        case RelationType::LabelFor: return 1;
        case RelationType::LabelledBy: return 2;
        case RelationType::ControllerFor: return 3;
        case RelationType::ControlledBy: return 4;
        case RelationType::MemberOf: return 5;
        case RelationType::NodeChildOf: return 7;
        case RelationType::FlowsTo: return 10;
        case RelationType::FlowsFrom: return 11;
        case RelationType::SubwindowOf: return 12;
        case RelationType::DescriptionFor: return 17;
        case RelationType::DescribedBy: return 18;
    }
    return 0;
}

int m_get_relation_set(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_open_container(rep.msg, 'a', "(ua(so))");
    if (!t.app) {
        for (const auto& [type, targets] : s->tree->get_node(t.id)->data().relations) {
            if (targets.empty()) continue;
            sd_bus_message_open_container(rep.msg, 'r', "ua(so)");
            sd_bus_message_append(rep.msg, "u", relation_code(static_cast<RelationType>(type)));
            sd_bus_message_open_container(rep.msg, 'a', "(so)");
            for (NodeId target : targets) s->append_ref(rep.msg, target);
            sd_bus_message_close_container(rep.msg);
            sd_bus_message_close_container(rep.msg);
        }
    }
    rep.r = sd_bus_message_close_container(rep.msg);
    return rep.send();
}

int m_get_role(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    uint32_t role = t.app ? 75u : role_to_atspi_role(s->tree->get_node(t.id)->role());
    return sd_bus_reply_method_return(m, "u", role);
}

int m_get_role_name(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    std::string name(t.app ? "application" : role_to_atspi_name(s->tree->get_node(t.id)->role()));
    return sd_bus_reply_method_return(m, "s", name.c_str());
}

int m_get_state(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    auto [lo, hi] = s->states_of(t);
    return sd_bus_reply_method_return(m, "au", 2, lo, hi);
}

int m_get_attributes(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_open_container(rep.msg, 'a', "{ss}");
    sd_bus_message_append(rep.msg, "{ss}", "toolkit", s->config.toolkit_name.c_str());
    if (!t.app) {
        for (const auto& [k, v] : s->tree->get_node(t.id)->attributes()) {
            if (k != "toolkit") sd_bus_message_append(rep.msg, "{ss}", k.c_str(), v.c_str());
        }
    }
    rep.r = sd_bus_message_close_container(rep.msg);
    return rep.send();
}

int m_get_application(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    rep.r = s->append_app_ref(rep.msg);
    return rep.send();
}

int append_interfaces(sd_bus_message* m, unsigned ifaces) {
    sd_bus_message_open_container(m, 'a', "s");
    const std::pair<unsigned, const char*> all[] = {
        {kAccessible, kIfaceAccessible}, {kApplication, kIfaceApplication}, {kComponent, kIfaceComponent},
        {kAction, kIfaceAction},         {kText, kIfaceText},               {kEditableText, kIfaceEditableText},
        {kValue, kIfaceValue},
    };
    for (const auto& [bit, name] : all) {
        if (ifaces & bit) sd_bus_message_append(m, "s", name);
    }
    return sd_bus_message_close_container(m);
}

int m_get_interfaces(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Target t;
    if (!target_of(s, path_of_msg(m), t, err)) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    rep.r = append_interfaces(rep.msg, s->interfaces_of(t));
    return rep.send();
}

const sd_bus_vtable kAccessibleVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Name", "s", prop_name, 0, 0),
    SD_BUS_PROPERTY("Description", "s", prop_description, 0, 0),
    SD_BUS_PROPERTY("Parent", "(so)", prop_parent, 0, 0),
    SD_BUS_PROPERTY("ChildCount", "i", prop_child_count, 0, 0),
    SD_BUS_PROPERTY("Locale", "s", prop_locale, 0, 0),
    SD_BUS_PROPERTY("AccessibleId", "s", prop_accessible_id, 0, 0),
    SD_BUS_METHOD("GetChildAtIndex", "i", "(so)", m_get_child_at_index, 0),
    SD_BUS_METHOD("GetChildren", "", "a(so)", m_get_children, 0),
    SD_BUS_METHOD("GetIndexInParent", "", "i", m_get_index_in_parent, 0),
    SD_BUS_METHOD("GetRelationSet", "", "a(ua(so))", m_get_relation_set, 0),
    SD_BUS_METHOD("GetRole", "", "u", m_get_role, 0),
    SD_BUS_METHOD("GetRoleName", "", "s", m_get_role_name, 0),
    SD_BUS_METHOD("GetLocalizedRoleName", "", "s", m_get_role_name, 0),
    SD_BUS_METHOD("GetState", "", "au", m_get_state, 0),
    SD_BUS_METHOD("GetAttributes", "", "a{ss}", m_get_attributes, 0),
    SD_BUS_METHOD("GetApplication", "", "(so)", m_get_application, 0),
    SD_BUS_METHOD("GetInterfaces", "", "as", m_get_interfaces, 0),
    SD_BUS_VTABLE_END,
};

// ── org.a11y.atspi.Application ───────────────────────────────────────────────

int prop_toolkit(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* u, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", srv(u)->config.toolkit_name.c_str());
}

int prop_version(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*, sd_bus_error*) {
    return sd_bus_message_append(reply, "s", std::string(kVersionString).c_str());
}

int prop_atspi_version(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void*,
                       sd_bus_error*) {
    return sd_bus_message_append(reply, "s", "2.1");
}

int prop_app_id(sd_bus*, const char*, const char*, const char*, sd_bus_message* reply, void* u, sd_bus_error*) {
    return sd_bus_message_append(reply, "i", srv(u)->app_id);
}

int set_app_id(sd_bus*, const char*, const char*, const char*, sd_bus_message* value, void* u, sd_bus_error*) {
    return sd_bus_message_read(value, "i", &srv(u)->app_id);
}

int m_get_locale(sd_bus_message* m, void*, sd_bus_error*) {
    uint32_t lctype = 0;
    int r = sd_bus_message_read(m, "u", &lctype);
    if (r < 0) return r;
    return sd_bus_reply_method_return(m, "s", "");
}

int m_get_app_bus_address(sd_bus_message* m, void*, sd_bus_error*) {
    // No peer-to-peer channel: clients stay on the accessibility bus.
    return sd_bus_reply_method_return(m, "s", "");
}

const sd_bus_vtable kApplicationVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("ToolkitName", "s", prop_toolkit, 0, 0),
    SD_BUS_PROPERTY("Version", "s", prop_version, 0, 0),
    SD_BUS_PROPERTY("AtspiVersion", "s", prop_atspi_version, 0, 0),
    SD_BUS_WRITABLE_PROPERTY("Id", "i", prop_app_id, set_app_id, 0, 0),
    SD_BUS_METHOD("GetLocale", "u", "s", m_get_locale, 0),
    SD_BUS_METHOD("GetApplicationBusAddress", "", "s", m_get_app_bus_address, 0),
    SD_BUS_VTABLE_END,
};

// ── org.a11y.atspi.Component ─────────────────────────────────────────────────

int m_contains(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Node* n = node_at(s, path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t x = 0, y = 0;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "iiu", &x, &y, &ct);
    if (r < 0) return r;
    return reply_bool(m, n->bounds().contains(to_window(s, n, x, y, ct)));
}

int m_accessible_at_point(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Node* n = node_at(s, path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t x = 0, y = 0;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "iiu", &x, &y, &ct);
    if (r < 0) return r;
    NodeId hit = s->tree->hit_test_from(n->id(), to_window(s, n, x, y, ct));
    Reply rep(m);
    rep.r = s->append_ref(rep.msg, hit);
    return rep.send();
}

int m_get_extents(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Node* n = node_at(s, path_of_msg(m), err);
    if (!n) return -ENOENT;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "u", &ct);
    if (r < 0) return r;
    RectF b = rect_in(s, n, ct);
    return sd_bus_reply_method_return(m, "(iiii)", ri(b.x), ri(b.y), ri(b.width), ri(b.height));
}

int m_get_position(sd_bus_message* m, void* u, sd_bus_error* err) {
    Server* s = srv(u);
    Node* n = node_at(s, path_of_msg(m), err);
    if (!n) return -ENOENT;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "u", &ct);
    if (r < 0) return r;
    RectF b = rect_in(s, n, ct);
    return sd_bus_reply_method_return(m, "ii", ri(b.x), ri(b.y));
}

int m_get_size(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return sd_bus_reply_method_return(m, "ii", ri(n->bounds().width), ri(n->bounds().height));
}

int m_get_layer(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    Role r = n->role();
    bool window = r == Role::Window || r == Role::Dialog || r == Role::Alert;
    return sd_bus_reply_method_return(m, "u", window ? 7u : 3u);  // ATSPI_LAYER_WINDOW / _WIDGET
}

int m_get_mdi_z_order(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "n", static_cast<int16_t>(-1));
}

int m_grab_focus(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return reply_bool(m, perform(n, kActionFocus));
}

int m_get_alpha(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "d", 1.0);
}

int m_refuse_geometry(sd_bus_message* m, void*, sd_bus_error*) {
    // Moving and resizing widgets is the application's business, not a client's.
    return reply_bool(m, false);
}

int m_scroll_to(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return reply_bool(m, perform(n, kActionScroll));
}

const sd_bus_vtable kComponentVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Contains", "iiu", "b", m_contains, 0),
    SD_BUS_METHOD("GetAccessibleAtPoint", "iiu", "(so)", m_accessible_at_point, 0),
    SD_BUS_METHOD("GetExtents", "u", "(iiii)", m_get_extents, 0),
    SD_BUS_METHOD("GetPosition", "u", "ii", m_get_position, 0),
    SD_BUS_METHOD("GetSize", "", "ii", m_get_size, 0),
    SD_BUS_METHOD("GetLayer", "", "u", m_get_layer, 0),
    SD_BUS_METHOD("GetMDIZOrder", "", "n", m_get_mdi_z_order, 0),
    SD_BUS_METHOD("GrabFocus", "", "b", m_grab_focus, 0),
    SD_BUS_METHOD("GetAlpha", "", "d", m_get_alpha, 0),
    SD_BUS_METHOD("SetExtents", "iiiiu", "b", m_refuse_geometry, 0),
    SD_BUS_METHOD("SetPosition", "iiu", "b", m_refuse_geometry, 0),
    SD_BUS_METHOD("SetSize", "ii", "b", m_refuse_geometry, 0),
    SD_BUS_METHOD("ScrollTo", "u", "b", m_scroll_to, 0),
    SD_BUS_METHOD("ScrollToPoint", "uii", "b", m_scroll_to, 0),
    SD_BUS_VTABLE_END,
};

// ── org.a11y.atspi.Action ────────────────────────────────────────────────────

int prop_n_actions(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                   sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    return sd_bus_message_append(reply, "i", static_cast<int32_t>(n->actions().size()));
}

const ActionDescriptor* action_at(sd_bus_message* m, Node* n, sd_bus_error* err) {
    int32_t i = -1;
    if (sd_bus_message_read(m, "i", &i) < 0 || i < 0 || static_cast<size_t>(i) >= n->actions().size()) {
        sd_bus_error_setf(err, SD_BUS_ERROR_INVALID_ARGS, "no action at index %d", i);
        return nullptr;
    }
    return &n->actions()[static_cast<size_t>(i)];
}

template <std::string ActionDescriptor::*Field>
int m_action_field(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    const ActionDescriptor* a = action_at(m, n, err);
    if (!a) return -EINVAL;
    return sd_bus_reply_method_return(m, "s", (a->*Field).c_str());
}

int m_get_actions(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_open_container(rep.msg, 'a', "(sss)");
    for (const auto& a : n->actions()) {
        sd_bus_message_append(rep.msg, "(sss)", a.name.c_str(), a.description.c_str(), a.key_binding.c_str());
    }
    rep.r = sd_bus_message_close_container(rep.msg);
    return rep.send();
}

int m_do_action(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    const ActionDescriptor* a = action_at(m, n, err);
    if (!a) return -EINVAL;
    std::string name = a->name;  // the handler may change the node's actions
    return reply_bool(m, perform(n, name));
}

const sd_bus_vtable kActionVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NActions", "i", prop_n_actions, 0, 0),
    SD_BUS_METHOD("GetDescription", "i", "s", m_action_field<&ActionDescriptor::description>, 0),
    SD_BUS_METHOD("GetName", "i", "s", m_action_field<&ActionDescriptor::name>, 0),
    SD_BUS_METHOD("GetLocalizedName", "i", "s", m_action_field<&ActionDescriptor::name>, 0),
    SD_BUS_METHOD("GetKeyBinding", "i", "s", m_action_field<&ActionDescriptor::key_binding>, 0),
    SD_BUS_METHOD("GetActions", "", "a(sss)", m_get_actions, 0),
    SD_BUS_METHOD("DoAction", "i", "b", m_do_action, 0),
    SD_BUS_VTABLE_END,
};

// ── org.a11y.atspi.Value ─────────────────────────────────────────────────────

template <double ValueRange::*Field>
int prop_value_field(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                     sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    ValueRange v = n->value().value_or(ValueRange{});
    return sd_bus_message_append(reply, "d", v.*Field);
}

int set_current_value(sd_bus*, const char* path, const char*, const char*, sd_bus_message* value, void* u,
                      sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    double d = 0;
    int r = sd_bus_message_read(value, "d", &d);
    if (r < 0) return r;
    ActionParams p;
    p.number_val = d;
    if (!perform(n, kActionSetValue, p)) {
        return sd_bus_error_set(err, SD_BUS_ERROR_ACCESS_DENIED, "the application did not accept the value");
    }
    return 0;
}

int prop_value_text(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                    sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    auto t = n->get_attribute("valuetext");
    return sd_bus_message_append(reply, "s", t ? t->c_str() : "");
}

const sd_bus_vtable kValueVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("MinimumValue", "d", prop_value_field<&ValueRange::minimum>, 0, 0),
    SD_BUS_PROPERTY("MaximumValue", "d", prop_value_field<&ValueRange::maximum>, 0, 0),
    SD_BUS_PROPERTY("MinimumIncrement", "d", prop_value_field<&ValueRange::step>, 0, 0),
    SD_BUS_WRITABLE_PROPERTY("CurrentValue", "d", prop_value_field<&ValueRange::current>, set_current_value, 0, 0),
    SD_BUS_PROPERTY("Text", "s", prop_value_text, 0, 0),
    SD_BUS_VTABLE_END,
};

// ── org.a11y.atspi.Cache ─────────────────────────────────────────────────────

int m_cache_get_items(sd_bus_message* m, void* u, sd_bus_error*) {
    Server* s = srv(u);
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_open_container(rep.msg, 'a', "((so)(so)(so)iiassusau)");
    s->append_cache_item(rep.msg, Target{true, kInvalidNodeId});
    s->tree->for_each_dfs([&](const Node* n) {
        s->append_cache_item(rep.msg, Target{false, n->id()});
        return true;
    });
    rep.r = sd_bus_message_close_container(rep.msg);
    return rep.send();
}

const sd_bus_vtable kCacheVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetItems", "", "a((so)(so)(so)iiassusau)", m_cache_get_items, 0),
    SD_BUS_SIGNAL("AddAccessible", "((so)(so)(so)iiassusau)", 0),
    SD_BUS_SIGNAL("RemoveAccessible", "(so)", 0),
    SD_BUS_VTABLE_END,
};

// ── registration ─────────────────────────────────────────────────────────────

template <unsigned Bit>
int find_with(sd_bus*, const char* path, const char*, void* userdata, void** found, sd_bus_error*) {
    Server* s = srv(userdata);
    Target t;
    if (!s->resolve(path, t) || !(s->interfaces_of(t) & Bit)) return 0;
    *found = userdata;
    return 1;
}

int enumerate(sd_bus*, const char*, void* userdata, char*** nodes, sd_bus_error*) {
    Server* s = srv(userdata);
    size_t count = s->tree->node_count() + 1;
    char** v = static_cast<char**>(calloc(count + 1, sizeof(char*)));
    if (!v) return -ENOMEM;
    size_t i = 0;
    v[i++] = strdup(kAppPath);
    s->tree->for_each_dfs([&](const Node* n) {
        if (i < count) v[i++] = strdup(s->path_of(n->id()).c_str());
        return true;
    });
    *nodes = v;
    return 0;
}

} // namespace

// Text and EditableText live in atspi_text.cpp.
extern const sd_bus_vtable kTextVtable[];
extern const sd_bus_vtable kEditableTextVtable[];

int register_objects(Server& s) {
    struct Entry {
        const char* iface;
        const sd_bus_vtable* vtable;
        sd_bus_object_find_t find;
    };
    const Entry entries[] = {
        {kIfaceAccessible, kAccessibleVtable, find_with<kAccessible>},
        {kIfaceApplication, kApplicationVtable, find_with<kApplication>},
        {kIfaceComponent, kComponentVtable, find_with<kComponent>},
        {kIfaceAction, kActionVtable, find_with<kAction>},
        {kIfaceText, kTextVtable, find_with<kText>},
        {kIfaceEditableText, kEditableTextVtable, find_with<kEditableText>},
        {kIfaceValue, kValueVtable, find_with<kValue>},
    };
    for (const Entry& e : entries) {
        sd_bus_slot* slot = nullptr;
        int r = sd_bus_add_fallback_vtable(s.bus, &slot, kPathPrefix, e.iface, e.vtable, e.find, &s);
        if (r < 0) return r;
        s.slots.push_back(slot);
    }
    sd_bus_slot* slot = nullptr;
    int r = sd_bus_add_object_vtable(s.bus, &slot, kCachePath, kIfaceCache, kCacheVtable, &s);
    if (r < 0) return r;
    s.slots.push_back(slot);
    r = sd_bus_add_node_enumerator(s.bus, &slot, kPathPrefix, enumerate, &s);
    if (r < 0) return r;
    s.slots.push_back(slot);
    return 0;
}

} // namespace broa11y::atspi
