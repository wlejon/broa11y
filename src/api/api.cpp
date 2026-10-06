#include "api.h"
#include "host_a11y_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace broa11y::api {

namespace {

std::mutex g_tree_mu;
std::shared_ptr<broa11y::Tree> g_custom_tree;
std::shared_ptr<broa11y::Tree> g_default_tree;

std::mutex g_bridge_mu;
std::shared_ptr<broa11y::Bridge> g_custom_bridge;
std::shared_ptr<broa11y::Bridge> g_default_bridge;
bool g_bridge_attempted = false;

std::mutex g_listener_id_mu;
EventListenerId g_tree_listener_id = 0;
broa11y::Tree* g_listened_tree = nullptr;

std::mutex g_events_mu;
std::vector<Event> g_queued_events;

struct A11yListener {
    uint64_t id = 0;
    std::string event_type;
    std::shared_ptr<ev::Persistent> callback;
};

std::mutex g_listeners_mu;
uint64_t g_next_listener_id = 1;
std::vector<A11yListener> g_listeners;

Value eventToJs(const Event& evItem) {
    ObjectBuilder b;
    b.set("type", std::string(event_type_to_string(evItem.type)));
    b.set("nodeId", static_cast<double>(evItem.node_id));
    b.set("node_id", static_cast<double>(evItem.node_id));

    switch (evItem.type) {
        case EventType::Announcement: {
            if (const auto* p = evItem.get_if<AnnouncementPayload>()) {
                b.set("message", p->message);
                b.set("priority", p->priority == AnnouncementPriority::Assertive ? "assertive" : "polite");
            }
            break;
        }
        case EventType::FocusChanged: {
            if (const auto* p = evItem.get_if<FocusChangedPayload>()) {
                b.set("previousFocusedId", static_cast<double>(p->previous_focused_id));
                b.set("currentFocusedId", static_cast<double>(p->current_focused_id));
            }
            break;
        }
        case EventType::StateChanged: {
            if (const auto* p = evItem.get_if<StateChangedPayload>()) {
                b.set("state", stateToString(p->state));
                b.set("enabled", p->enabled);
            }
            break;
        }
        case EventType::PropertyChanged: {
            if (const auto* p = evItem.get_if<PropertyChangedPayload>()) {
                b.set("propertyName", p->property_name);
                b.set("oldValue", p->old_value);
                b.set("newValue", p->new_value);
            }
            break;
        }
        case EventType::BoundsChanged: {
            if (const auto* p = evItem.get_if<BoundsChangedPayload>()) {
                ObjectBuilder ob, nb;
                ob.set("x", p->old_bounds.x);
                ob.set("y", p->old_bounds.y);
                ob.set("width", p->old_bounds.width);
                ob.set("height", p->old_bounds.height);
                nb.set("x", p->new_bounds.x);
                nb.set("y", p->new_bounds.y);
                nb.set("width", p->new_bounds.width);
                nb.set("height", p->new_bounds.height);
                b.set("oldBounds", ob.build());
                b.set("newBounds", nb.build());
            }
            break;
        }
        case EventType::ValueChanged: {
            if (const auto* p = evItem.get_if<ValueChangedPayload>()) {
                b.set("current", p->new_value.current);
                b.set("minimum", p->new_value.minimum);
                b.set("maximum", p->new_value.maximum);
                b.set("step", p->new_value.step);
            }
            break;
        }
        case EventType::CaretMoved: {
            if (const auto* p = evItem.get_if<CaretMovedPayload>()) {
                b.set("oldOffset", static_cast<double>(p->old_offset));
                b.set("newOffset", static_cast<double>(p->new_offset));
            }
            break;
        }
        case EventType::TextSelectionChanged: {
            if (const auto* p = evItem.get_if<TextSelectionPayload>()) {
                b.set("startOffset", static_cast<double>(p->selection.start_offset));
                b.set("endOffset", static_cast<double>(p->selection.end_offset));
            }
            break;
        }
        case EventType::ChildrenChanged: {
            if (const auto* p = evItem.get_if<ChildrenChangedPayload>()) {
                b.set("changeType", p->change_type == ChildrenChangeType::ChildAdded ? "child_added" : "child_removed");
                b.set("childId", static_cast<double>(p->child_id));
                b.set("index", static_cast<double>(p->index));
            }
            break;
        }
        default:
            break;
    }
    return b.build();
}

} // namespace

void queueA11yEvent(const Event& event) {
    std::lock_guard lock(g_events_mu);
    g_queued_events.push_back(event);
}

void ensureTreeListener(broa11y::Tree* tree) {
    std::lock_guard lock(g_listener_id_mu);
    if (!tree) return;
    if (g_listened_tree == tree && g_tree_listener_id != 0) return;
    if (g_listened_tree && g_tree_listener_id != 0) {
        g_listened_tree->remove_listener(g_tree_listener_id);
        g_tree_listener_id = 0;
        g_listened_tree = nullptr;
    }
    g_tree_listener_id = tree->add_listener([](const Event& event) {
        queueA11yEvent(event);
    });
    g_listened_tree = tree;
}

void removeTreeListener() {
    std::lock_guard lock(g_listener_id_mu);
    if (g_listened_tree && g_tree_listener_id != 0) {
        g_listened_tree->remove_listener(g_tree_listener_id);
    }
    g_tree_listener_id = 0;
    g_listened_tree = nullptr;
}

std::shared_ptr<broa11y::Tree> activeTree() {
    std::lock_guard lock(g_tree_mu);
    if (g_custom_tree) return g_custom_tree;
    if (!g_default_tree) {
        g_default_tree = std::make_shared<broa11y::Tree>();
        auto* app = g_default_tree->create_node_with_role(broa11y::Role::Application, 1);
        if (app) {
            app->set_name("bro");
        }
        ensureTreeListener(g_default_tree.get());
    }
    return g_default_tree;
}

void setTree(std::shared_ptr<broa11y::Tree> tree) {
    std::lock_guard lock(g_tree_mu);
    g_custom_tree = std::move(tree);
    if (g_custom_tree) {
        ensureTreeListener(g_custom_tree.get());
    } else if (g_default_tree) {
        ensureTreeListener(g_default_tree.get());
    }
}

std::shared_ptr<broa11y::Tree> getTree() {
    return activeTree();
}

std::shared_ptr<broa11y::Bridge> activeBridge() {
    std::lock_guard lock(g_bridge_mu);
    if (g_custom_bridge) return g_custom_bridge;
    if (!g_default_bridge && !g_bridge_attempted) {
        g_bridge_attempted = true;
#if defined(_WIN32)
        // WinBridge requires HWND, left for setBridge
#elif defined(__APPLE__)
        // MacBridge requires NSView, left for setBridge
#elif defined(__linux__)
        auto b = std::make_shared<broa11y::LinuxBridge>();
        auto tree = activeTree();
        if (tree) {
            b->initialize(tree.get());
        }
        g_default_bridge = b;
#endif
    }
    return g_default_bridge;
}

void setBridge(std::shared_ptr<broa11y::Bridge> bridge) {
    std::lock_guard lock(g_bridge_mu);
    g_custom_bridge = std::move(bridge);
}

std::shared_ptr<broa11y::Bridge> getBridge() {
    return activeBridge();
}

bool available() {
    auto b = activeBridge();
    if (b && b->is_active()) return true;
#if defined(_WIN32)
    return true;
#elif defined(__APPLE__)
    return true;
#elif defined(__linux__)
    if (const char* env = std::getenv("AT_SPI_BUS_ADDRESS"); env && *env) return true;
    if (const char* session = std::getenv("DBUS_SESSION_BUS_ADDRESS"); session && *session) return true;
    return false;
#else
    return false;
#endif
}

void clearA11yListeners() {
    std::lock_guard lock(g_listeners_mu);
    g_listeners.clear();
}

void drainA11yEvents() {
    auto bridge = activeBridge();
    if (bridge && bridge->is_active()) {
        bridge->process_events();
    }

    std::vector<Event> events;
    {
        std::lock_guard lock(g_events_mu);
        events.swap(g_queued_events);
    }

    if (events.empty()) return;

    for (const auto& evItem : events) {
        ev::Persistent payloadP(eventToJs(evItem));
        std::string eventName = std::string(event_type_to_string(evItem.type));

        std::vector<std::string> names = { eventName, "*" };
        if (evItem.type == EventType::Announcement) {
            names.push_back("announce");
        } else if (evItem.type == EventType::FocusChanged) {
            names.push_back("nodeFocused");
            names.push_back("node_focused");
        } else if (evItem.type == EventType::StateChanged) {
            names.push_back("stateChanged");
        } else if (evItem.type == EventType::PropertyChanged) {
            names.push_back("propertyChanged");
        } else if (evItem.type == EventType::NodeAdded) {
            names.push_back("nodeAdded");
        } else if (evItem.type == EventType::NodeRemoved) {
            names.push_back("nodeRemoved");
        } else if (evItem.type == EventType::BoundsChanged) {
            names.push_back("boundsChanged");
        } else if (evItem.type == EventType::ValueChanged) {
            names.push_back("valueChanged");
        }

        std::vector<std::shared_ptr<ev::Persistent>> targets;
        {
            std::lock_guard lock(g_listeners_mu);
            for (const auto& l : g_listeners) {
                for (const auto& name : names) {
                    if (l.event_type == name) {
                        targets.push_back(l.callback);
                        break;
                    }
                }
            }
        }

        for (const auto& cb : targets) {
            if (cb && !ev::isUndefined(cb->get()) && ev::isFunction(cb->get())) {
                const Value arg = payloadP.get();
                ev::call(cb->get(), ev::undefined(), std::span<const Value>(&arg, 1));
            }
        }
    }
}

Value ensureBroA11y() {
    ev::Persistent globalThisVal;
    auto gt = ev::globalValue("globalThis");
    if (gt.found && ev::isObject(gt.value)) {
        globalThisVal.set(gt.value);
    }

    ev::Persistent broP;
    auto bro = ev::globalValue("bro");
    if (bro.found && ev::isObject(bro.value)) broP.set(bro.value);
    if (!ev::isObject(broP.get()) && ev::isObject(globalThisVal.get())) {
        Value candidate = ev::getProperty(globalThisVal.get(), "bro");
        if (ev::isObject(candidate)) broP.set(candidate);
    }
    if (!ev::isObject(broP.get())) {
        broP.set(ev::createObject());
        ev::registerGlobal("bro", broP.get());
        if (ev::isObject(globalThisVal.get())) {
            globalThisVal.set(ev::setProperty(globalThisVal.get(), "bro", broP.get()));
        }
    }

    ev::Persistent a11yP(ev::getProperty(broP.get(), "a11y"));
    if (!ev::isObject(a11yP.get())) {
        a11yP.set(ev::createObject());
        broP.set(ev::setProperty(broP.get(), "a11y", a11yP.get()));
    }
    return a11yP.get();
}

void installA11y() {
    ev::Persistent a11yObj(ensureBroA11y());
    ObjectBuilder a11y(a11yObj.get());

    a11y.set("available", available());
    a11y.def("isAvailable", 0, [](Value, std::span<const Value>) -> Value {
        return ev::fromBool(available());
    });


    // bro.a11y.tick() -> void
    a11y.def("tick", 0, [](Value, std::span<const Value>) -> Value {
        tickA11yAsync();
        return ev::undefined();
    });

    // bro.a11y.shutdown() -> void
    a11y.def("shutdown", 0, [](Value, std::span<const Value>) -> Value {
        shutdownA11yAsync();
        return ev::undefined();
    });

    // bro.a11y.on(eventType, handler) -> HandlerHandle
    auto onFn = [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0) || !reader.isFunction(1)) {
            return ev::fromDouble(0.0);
        }

        std::string event = reader.getString(0);
        uint64_t id = 0;
        {
            std::lock_guard lock(g_listeners_mu);
            id = g_next_listener_id++;
            A11yListener l;
            l.id = id;
            l.event_type = event;
            l.callback = std::make_shared<ev::Persistent>(reader.get(1));
            g_listeners.push_back(std::move(l));
        }

        ObjectBuilder handle;
        handle.set("id", static_cast<double>(id));
        handle.set("event", event);
        handle.def("remove", 0, [id](Value, std::span<const Value>) -> Value {
            std::lock_guard lock(g_listeners_mu);
            size_t removed = std::erase_if(g_listeners, [id](const auto& l) { return l.id == id; });
            return ev::fromBool(removed > 0);
        });
        return handle.build();
    };

    a11y.def("on", 2, onFn);
    a11y.def("addEventListener", 2, onFn);
    a11y.def("addListener", 2, onFn);

    // bro.a11y.off(eventOrHandle, callback?) -> boolean
    auto offFn = [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        std::lock_guard lock(g_listeners_mu);
        if (reader.count() == 0) {
            bool hadAny = !g_listeners.empty();
            g_listeners.clear();
            return ev::fromBool(hadAny);
        }

        if (reader.isObject(0)) {
            ev::Persistent objP(reader.get(0));
            double id = ArgReader::getPropDouble(objP.get(), "id", 0.0);
            if (id > 0) {
                uint64_t targetId = static_cast<uint64_t>(id);
                size_t removed = std::erase_if(g_listeners, [targetId](const auto& l) {
                    return l.id == targetId;
                });
                return ev::fromBool(removed > 0);
            }
        }

        if (reader.isString(0)) {
            std::string event = reader.getString(0);
            if (reader.isFunction(1)) {
                Value targetCb = reader.get(1);
                size_t removed = std::erase_if(g_listeners, [&](const auto& l) {
                    return l.event_type == event && l.callback && (l.callback->get() == targetCb);
                });
                return ev::fromBool(removed > 0);
            }
            size_t removed = std::erase_if(g_listeners, [&](const auto& l) {
                return l.event_type == event;
            });
            return ev::fromBool(removed > 0);
        }

        return ev::fromBool(false);
    };

    a11y.def("off", 1, offFn);
    a11y.def("removeEventListener", 1, offFn);
    a11y.def("removeListener", 1, offFn);

    // Mount sub-modules
    installTreeOnto(a11y.get());
    installAnnounceOnto(a11y.get());
    installCustomOnto(a11y.get());
}

void tickA11yAsync() {
    drainA11yEvents();
}

void shutdownA11yAsync() {
    clearA11yListeners();
    clearCustomRoles();
    removeTreeListener();
    {
        std::lock_guard lock(g_events_mu);
        g_queued_events.clear();
    }
}

} // namespace broa11y::api
