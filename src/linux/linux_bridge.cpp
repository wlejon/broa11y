#include "broa11y/linux_bridge.h"
#include "common/text_util.h"
#include "linux/atspi_server.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace broa11y {

namespace atspi {

std::string Server::path_of(NodeId id) const {
    return std::string(kPathPrefix) + "/" + std::to_string(id);
}

bool Server::resolve(const char* path, Target& out) const {
    if (!tree || !path) return false;
    std::string_view p(path);
    if (p == kAppPath) {
        out = Target{true, kInvalidNodeId};
        return true;
    }
    std::string_view prefix(kPathPrefix);
    if (p.size() <= prefix.size() + 1 || p.substr(0, prefix.size()) != prefix || p[prefix.size()] != '/') {
        return false;
    }
    NodeId id = 0;
    for (char c : p.substr(prefix.size() + 1)) {
        if (c < '0' || c > '9') return false;
        id = id * 10 + static_cast<NodeId>(c - '0');
    }
    if (id == kInvalidNodeId || !tree->contains_node(id)) return false;
    out = Target{false, id};
    return true;
}

unsigned Server::interfaces_of(const Target& t) const {
    if (t.app) return kAccessible | kApplication;
    const Node* n = tree->get_node(t.id);
    if (!n) return 0;
    unsigned ifaces = kAccessible | kComponent;
    const Role r = n->role();
    if (!n->actions().empty()) ifaces |= kAction;
    if (r == Role::TextInput || r == Role::Terminal || r == Role::Document || !n->text().empty()) ifaces |= kText;
    if (r == Role::TextInput && !n->has_state(State::ReadOnly)) ifaces |= kEditableText;
    if (n->value()) ifaces |= kValue;
    return ifaces;
}

PointF Server::window_origin() const {
    return config.window_origin ? config.window_origin() : PointF{};
}

std::pair<uint32_t, uint32_t> Server::states_of(const Target& t) const {
    if (t.app) return {0, 0};
    const Node* n = tree->get_node(t.id);
    if (!n) return {0, 0};
    auto [lo, hi] = n->states().to_atspi_state_bitmask();
    // Laid-out nodes are on screen: VISIBLE and SHOWING, without which Orca
    // skips them. The model can still set them explicitly.
    if (!n->bounds().is_empty()) lo |= (1u << 30) | (1u << 25);
    if (n->role() == Role::CheckBox || n->role() == Role::RadioButton) hi |= 1u << (41 - 32);  // CHECKABLE
    return {lo, hi};
}

int Server::append_ref(sd_bus_message* m, NodeId id) const {
    if (id != kInvalidNodeId && tree->contains_node(id)) {
        return sd_bus_message_append(m, "(so)", unique_name.c_str(), path_of(id).c_str());
    }
    return sd_bus_message_append(m, "(so)", unique_name.c_str(), kNullPath);
}

int Server::append_app_ref(sd_bus_message* m) const {
    return sd_bus_message_append(m, "(so)", unique_name.c_str(), kAppPath);
}

int Server::append_cache_item(sd_bus_message* m, const Target& t) const {
    int r = sd_bus_message_open_container(m, 'r', "(so)(so)(so)iiassusau");
    if (r < 0) return r;
    const Node* n = t.app ? nullptr : tree->get_node(t.id);
    if (t.app) {
        append_app_ref(m);
    } else {
        append_ref(m, t.id);
    }
    append_app_ref(m);
    if (t.app) {
        sd_bus_message_append(m, "(so)", desktop_bus.c_str(), desktop_path.c_str());
    } else if (t.id == tree->root_id()) {
        append_app_ref(m);
    } else {
        append_ref(m, n->parent_id());
    }
    int32_t index = t.app ? -1 : (t.id == tree->root_id() ? 0 : n->index_in_parent());
    int32_t children = t.app ? (tree->root() ? 1 : 0) : static_cast<int32_t>(n->child_count());
    sd_bus_message_append(m, "ii", index, children);

    sd_bus_message_open_container(m, 'a', "s");
    const std::pair<unsigned, const char*> all[] = {
        {kAccessible, kIfaceAccessible}, {kApplication, kIfaceApplication}, {kComponent, kIfaceComponent},
        {kAction, kIfaceAction},         {kText, kIfaceText},               {kEditableText, kIfaceEditableText},
        {kValue, kIfaceValue},
    };
    const unsigned ifaces = interfaces_of(t);
    for (const auto& [bit, name] : all) {
        if (ifaces & bit) sd_bus_message_append(m, "s", name);
    }
    sd_bus_message_close_container(m);

    std::string name = t.app ? config.app_name : n->name();
    uint32_t role = t.app ? 75u : role_to_atspi_role(n->role());
    std::string desc = t.app ? std::string() : n->description();
    sd_bus_message_append(m, "sus", name.c_str(), role, desc.c_str());
    auto [lo, hi] = states_of(t);
    sd_bus_message_append(m, "au", 2, lo, hi);
    return sd_bus_message_close_container(m);
}

int Server::emit(NodeId id, const char* iface, const char* member, const std::string& detail, int32_t d1,
                 int32_t d2, const std::function<int(sd_bus_message*)>& any) const {
    if (!bus) return 0;
    sd_bus_message* m = nullptr;
    std::string path = path_of(id);
    int r = sd_bus_message_new_signal(bus, &m, path.c_str(), iface, member);
    if (r < 0) return r;
    r = sd_bus_message_append(m, "sii", detail.c_str(), d1, d2);
    if (r >= 0) r = any(m);
    if (r >= 0) r = sd_bus_message_append(m, "a{sv}", 0);
    if (r >= 0) r = sd_bus_send(bus, m, nullptr);
    sd_bus_message_unref(m);
    return r;
}

const char* state_name(State s) {
    switch (s) {
        case State::Focused: return "focused";
        case State::Focusable: return "focusable";
        case State::Selected: return "selected";
        case State::Selectable: return "selectable";
        case State::Expanded: return "expanded";
        case State::Collapsed: return "collapsed";
        case State::Disabled: return "enabled";  // emitted inverted
        case State::ReadOnly: return "read-only";
        case State::Checked: return "checked";
        case State::Busy: return "busy";
        case State::Modal: return "modal";
        case State::MultiSelectable: return "multiselectable";
        case State::Visible: return "visible";
        case State::Showing: return "showing";
        case State::Sensitive: return "sensitive";
        case State::Defunct: return "defunct";
        case State::Active: return "active";
        case State::Armed: return "armed";
        case State::Indeterminate: return "indeterminate";
        case State::Vertical: return "vertical";
        case State::Horizontal: return "horizontal";
        case State::Required: return "required";
        case State::Invalid: return "invalid-entry";
        case State::MultiLine: return "multi-line";
        case State::SingleLine: return "single-line";
        case State::HasPopup: return "has-popup";
        case State::SelectableText: return "selectable-text";
        case State::Editable: return "editable";
        case State::Animated: return "animated";
        case State::Transient: return "transient";
        case State::Count: break;
    }
    return "";
}

}  // namespace atspi

using atspi::kIfaceEventObject;

namespace {

int any_int(sd_bus_message* m) { return sd_bus_message_append(m, "v", "i", 0); }

}  // namespace

class LinuxBridge::Impl {
public:
    explicit Impl(LinuxBridgeConfig config) { s_.config = std::move(config); }
    ~Impl() { shutdown(); }

    bool initialize(Tree* tree);
    void shutdown();
    void handle_event(const Event& ev);

    void process() {
        if (!active_) return;
        int r;
        while ((r = sd_bus_process(s_.bus, nullptr)) > 0) {
        }
        if (r < 0) {
            error_ = std::string("the accessibility bus connection failed: ") + std::strerror(-r);
            shutdown();
        }
    }

    atspi::Server s_;
    EventListenerId listener_ = 0;
    bool active_ = false;
    std::string error_;

private:
    bool fail(std::string why) {
        error_ = std::move(why);
        close_bus();
        return false;
    }

    void close_bus() {
        for (sd_bus_slot* slot : s_.slots) sd_bus_slot_unref(slot);
        s_.slots.clear();
        if (s_.bus) s_.bus = sd_bus_flush_close_unref(s_.bus);
        s_.unique_name.clear();
    }

    std::string find_bus_address();
    bool embed();
    void emit_text_change(NodeId id, const std::string& before, const std::string& after);
};

std::string LinuxBridge::Impl::find_bus_address() {
    if (const char* env = std::getenv("AT_SPI_BUS_ADDRESS"); env && *env) return env;

    sd_bus* session = nullptr;
    int r = sd_bus_open_user(&session);
    if (r < 0) {
        error_ = std::string("no session bus to ask for the accessibility bus (") + std::strerror(-r) +
                 "); set DBUS_SESSION_BUS_ADDRESS or AT_SPI_BUS_ADDRESS";
        return {};
    }
    sd_bus_error err{};  // SD_BUS_ERROR_NULL, without its C compound literal
    sd_bus_message* reply = nullptr;
    std::string address;
    r = sd_bus_call_method(session, "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress", &err, &reply, "");
    if (r < 0) {
        error_ = std::string("org.a11y.Bus.GetAddress failed on the session bus (") +
                 (err.message ? err.message : std::strerror(-r)) + "); is at-spi2-core installed?";
    } else {
        const char* a = nullptr;
        if (sd_bus_message_read(reply, "s", &a) >= 0 && a && *a) {
            address = a;
        } else {
            error_ = "org.a11y.Bus.GetAddress returned no address";
        }
    }
    sd_bus_error_free(&err);
    sd_bus_message_unref(reply);
    sd_bus_flush_close_unref(session);
    return address;
}

bool LinuxBridge::Impl::embed() {
    // Asynchronous, with the bus processed while waiting: the registry may
    // call back into the application (Application.Id) before it answers.
    struct Result {
        bool done = false;
        bool ok = false;
        std::string desktop_bus, desktop_path, error;
    } result;
    auto on_reply = [](sd_bus_message* m, void* userdata, sd_bus_error*) -> int {
        auto* res = static_cast<Result*>(userdata);
        res->done = true;
        if (sd_bus_message_is_method_error(m, nullptr)) {
            const sd_bus_error* e = sd_bus_message_get_error(m);
            res->error = e && e->message ? e->message : "error reply";
            return 0;
        }
        const char* b = nullptr;
        const char* p = nullptr;
        if (sd_bus_message_read(m, "(so)", &b, &p) >= 0) {
            res->ok = true;
            res->desktop_bus = b ? b : "";
            res->desktop_path = p ? p : atspi::kNullPath;
        } else {
            res->error = "unexpected Embed reply";
        }
        return 0;
    };
    sd_bus_slot* slot = nullptr;
    int r = sd_bus_call_method_async(s_.bus, &slot, "org.a11y.atspi.Registry", atspi::kAppPath,
                                     "org.a11y.atspi.Socket", "Embed", on_reply, &result, "(so)",
                                     s_.unique_name.c_str(), atspi::kAppPath);
    if (r < 0) {
        error_ = std::string("cannot call Socket.Embed: ") + std::strerror(-r);
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!result.done && std::chrono::steady_clock::now() < deadline) {
        r = sd_bus_process(s_.bus, nullptr);
        if (r < 0) break;
        if (r == 0) sd_bus_wait(s_.bus, 100000);
    }
    sd_bus_slot_unref(slot);
    if (!result.ok) {
        error_ = "the AT-SPI registry did not accept Socket.Embed (" +
                 (result.done ? result.error : std::string("no answer in 10 s")) + ")";
        return false;
    }
    s_.desktop_bus = result.desktop_bus;
    s_.desktop_path = result.desktop_path;
    return true;
}

bool LinuxBridge::Impl::initialize(Tree* tree) {
    if (active_) return true;
    error_.clear();
    if (!tree) return fail("no tree");
    s_.tree = tree;

    std::string address = find_bus_address();
    if (address.empty()) return fail(error_);

    int r = sd_bus_new(&s_.bus);
    if (r >= 0) r = sd_bus_set_address(s_.bus, address.c_str());
    if (r >= 0) r = sd_bus_set_bus_client(s_.bus, 1);
    if (r >= 0) r = sd_bus_start(s_.bus);
    if (r < 0) {
        return fail("cannot connect to the accessibility bus at " + address + " (" + std::strerror(-r) + ")");
    }
    const char* unique = nullptr;
    if (sd_bus_get_unique_name(s_.bus, &unique) < 0 || !unique) return fail("no unique name on the accessibility bus");
    s_.unique_name = unique;

    r = atspi::register_objects(s_);
    if (r < 0) return fail(std::string("cannot export the accessible objects: ") + std::strerror(-r));
    if (!embed()) return fail(error_);

    listener_ = tree->add_listener([this](const Event& ev) { handle_event(ev); });
    active_ = true;
    return true;
}

void LinuxBridge::Impl::shutdown() {
    if (s_.tree && listener_) s_.tree->remove_listener(listener_);
    listener_ = 0;
    active_ = false;
    close_bus();
    s_.tree = nullptr;
}

void LinuxBridge::Impl::emit_text_change(NodeId id, const std::string& before, const std::string& after) {
    text::TextIndex a(before);
    text::TextIndex b(after);
    int32_t p = 0;
    while (p < a.size() && p < b.size() && a.at(p) == b.at(p)) ++p;
    int32_t q = 0;
    while (q < a.size() - p && q < b.size() - p && a.at(a.size() - 1 - q) == b.at(b.size() - 1 - q)) ++q;
    std::string removed = a.slice(p, a.size() - q);
    std::string added = b.slice(p, b.size() - q);
    if (!removed.empty()) {
        s_.emit(id, kIfaceEventObject, "TextChanged", "delete", p, a.size() - q - p,
                [&](sd_bus_message* m) { return sd_bus_message_append(m, "v", "s", removed.c_str()); });
    }
    if (!added.empty()) {
        s_.emit(id, kIfaceEventObject, "TextChanged", "insert", p, b.size() - q - p,
                [&](sd_bus_message* m) { return sd_bus_message_append(m, "v", "s", added.c_str()); });
    }
}

void LinuxBridge::Impl::handle_event(const Event& ev) {
    if (!active_) return;
    Tree* tree = s_.tree;
    const NodeId id = ev.node_id;
    switch (ev.type) {
        case EventType::PropertyChanged:
            if (const auto* p = ev.get_if<PropertyChangedPayload>()) {
                if (p->property_name == "name" || p->property_name == "description") {
                    std::string detail = "accessible-" + p->property_name;
                    s_.emit(id, kIfaceEventObject, "PropertyChange", detail, 0, 0, [&](sd_bus_message* m) {
                        return sd_bus_message_append(m, "v", "s", p->new_value.c_str());
                    });
                } else if (p->property_name == "role") {
                    uint32_t role = role_to_atspi_role(string_to_role(p->new_value));
                    s_.emit(id, kIfaceEventObject, "PropertyChange", "accessible-role", 0, 0,
                            [&](sd_bus_message* m) { return sd_bus_message_append(m, "v", "u", role); });
                } else if (p->property_name == "text") {
                    emit_text_change(id, p->old_value, p->new_value);
                }
            }
            break;

        case EventType::StateChanged:
            if (const auto* p = ev.get_if<StateChangedPayload>()) {
                // The model's "disabled" is AT-SPI's lack of enabled + sensitive.
                bool on = p->state == State::Disabled ? !p->enabled : p->enabled;
                s_.emit(id, kIfaceEventObject, "StateChanged", atspi::state_name(p->state), on ? 1 : 0, 0, any_int);
                if (p->state == State::Disabled) {
                    s_.emit(id, kIfaceEventObject, "StateChanged", "sensitive", on ? 1 : 0, 0, any_int);
                }
            }
            break;

        case EventType::ValueChanged:
            if (const auto* p = ev.get_if<ValueChangedPayload>()) {
                double v = p->new_value.current;
                s_.emit(id, kIfaceEventObject, "PropertyChange", "accessible-value", 0, 0,
                        [&](sd_bus_message* m) { return sd_bus_message_append(m, "v", "d", v); });
            }
            break;

        case EventType::BoundsChanged:
            if (const auto* p = ev.get_if<BoundsChangedPayload>()) {
                const RectF& b = p->new_bounds;
                s_.emit(id, kIfaceEventObject, "BoundsChanged", "", 0, 0, [&](sd_bus_message* m) {
                    return sd_bus_message_append(m, "v", "(iiii)", static_cast<int32_t>(b.x),
                                                 static_cast<int32_t>(b.y), static_cast<int32_t>(b.width),
                                                 static_cast<int32_t>(b.height));
                });
            }
            break;

        case EventType::CaretMoved:
            if (const auto* p = ev.get_if<CaretMovedPayload>()) {
                const Node* n = tree->get_node(id);
                if (!n) break;
                int32_t caret = p->new_offset < 0 ? -1 : text::TextIndex(n->text()).cp_from_byte(p->new_offset);
                s_.emit(id, kIfaceEventObject, "TextCaretMoved", "", caret, 0, any_int);
            }
            break;

        case EventType::TextSelectionChanged:
            s_.emit(id, kIfaceEventObject, "TextSelectionChanged", "", 0, 0, any_int);
            break;

        case EventType::ChildrenChanged:
            if (const auto* p = ev.get_if<ChildrenChangedPayload>()) {
                const bool added = p->change_type == ChildrenChangeType::ChildAdded;
                // A removed child is already gone from the tree; its path still names it.
                std::string child_path = s_.path_of(p->child_id);
                s_.emit(id, kIfaceEventObject, "ChildrenChanged", added ? "add" : "remove",
                        static_cast<int32_t>(p->index), 0, [&](sd_bus_message* m) {
                            return sd_bus_message_append(m, "v", "(so)", s_.unique_name.c_str(), child_path.c_str());
                        });
                sd_bus_message* sig = nullptr;
                if (sd_bus_message_new_signal(s_.bus, &sig, atspi::kCachePath, atspi::kIfaceCache,
                                              added ? "AddAccessible" : "RemoveAccessible") >= 0) {
                    int r = added ? s_.append_cache_item(sig, atspi::Target{false, p->child_id})
                                  : sd_bus_message_append(sig, "(so)", s_.unique_name.c_str(), child_path.c_str());
                    if (r >= 0) sd_bus_send(s_.bus, sig, nullptr);
                    sd_bus_message_unref(sig);
                }
            }
            break;

        case EventType::Announcement:
            if (const auto* p = ev.get_if<AnnouncementPayload>()) {
                NodeId origin = tree->contains_node(id) ? id : tree->root_id();
                // AtspiLive: POLITE = 1, ASSERTIVE = 2.
                int32_t politeness = p->priority == AnnouncementPriority::Assertive ? 2 : 1;
                s_.emit(origin, kIfaceEventObject, "Announcement", "", politeness, 0, [&](sd_bus_message* m) {
                    return sd_bus_message_append(m, "v", "s", p->message.c_str());
                });
            }
            break;

        case EventType::WindowActivated:
        case EventType::WindowDeactivated: {
            const Node* n = tree->get_node(id);
            std::string name = n ? n->name() : std::string();
            s_.emit(id, atspi::kIfaceEventWindow, ev.type == EventType::WindowActivated ? "Activate" : "Deactivate",
                    "", 0, 0, [&](sd_bus_message* m) { return sd_bus_message_append(m, "v", "s", name.c_str()); });
            break;
        }

        case EventType::FocusChanged:  // the Focused state change carries it
        case EventType::NodeAdded:     // ChildrenChanged carries these two
        case EventType::NodeRemoved:
        case EventType::Count:
            break;
    }
}

LinuxBridge::LinuxBridge(LinuxBridgeConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

LinuxBridge::~LinuxBridge() = default;

bool LinuxBridge::initialize(Tree* tree) { return impl_->initialize(tree); }

void LinuxBridge::shutdown() { impl_->shutdown(); }

void LinuxBridge::handle_event(const Event& event) { impl_->handle_event(event); }

void LinuxBridge::process_events() { impl_->process(); }

bool LinuxBridge::is_active() const noexcept { return impl_->active_; }

std::string LinuxBridge::last_error() const { return impl_->error_; }

int LinuxBridge::poll_fd() const { return impl_->active_ ? sd_bus_get_fd(impl_->s_.bus) : -1; }

short LinuxBridge::poll_events() const {
    if (!impl_->active_) return 0;
    int e = sd_bus_get_events(impl_->s_.bus);
    return e < 0 ? 0 : static_cast<short>(e);
}

int LinuxBridge::poll_timeout_ms() const {
    if (!impl_->active_) return -1;
    uint64_t usec = 0;
    if (sd_bus_get_timeout(impl_->s_.bus, &usec) < 0 || usec == UINT64_MAX) return -1;
    // sd_bus_get_timeout() is an absolute CLOCK_MONOTONIC time.
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = static_cast<uint64_t>(ts.tv_sec) * 1000000u + static_cast<uint64_t>(ts.tv_nsec) / 1000u;
    return usec <= now ? 0 : static_cast<int>((usec - now + 999) / 1000);
}

std::string LinuxBridge::bus_name() const { return impl_->s_.unique_name; }

} // namespace broa11y
