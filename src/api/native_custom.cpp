#include "host_a11y_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace broa11y::api {

namespace {

struct CustomRole {
    std::string name;
    std::string base_role = "canvas";
    std::string description;
    std::vector<std::string> supported_actions;
    std::vector<std::string> supported_states;
    std::shared_ptr<ev::Persistent> action_handler;
};

struct WidgetHook {
    uint64_t node_id = 0;
    std::shared_ptr<ev::Persistent> hooks;
};

std::mutex g_custom_mu;
std::unordered_map<std::string, CustomRole> g_custom_roles;
std::unordered_map<uint64_t, WidgetHook> g_widget_hooks;

} // namespace

void clearCustomRoles() {
    std::lock_guard lock(g_custom_mu);
    g_custom_roles.clear();
    g_widget_hooks.clear();
}

bool dispatchWidgetAction(NodeId id, std::string_view action, const ActionParams& params) {
    std::shared_ptr<ev::Persistent> hookObj;
    std::shared_ptr<ev::Persistent> roleHandler;

    {
        std::lock_guard lock(g_custom_mu);
        auto itHook = g_widget_hooks.find(id);
        if (itHook != g_widget_hooks.end()) {
            hookObj = itHook->second.hooks;
        }
    }

    if (hookObj && !ev::isUndefined(hookObj->get())) {
        ev::Persistent actFnP(ev::getProperty(hookObj->get(), "onAction"));
        if (!ev::isFunction(actFnP.get())) {
            actFnP.set(ev::getProperty(hookObj->get(), "actionHandler"));
        }
        if (ev::isFunction(actFnP.get())) {
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

            ev::Persistent actP(ev::fromUtf8(action));
            ev::Persistent pObj(pb.build());
            ev::Persistent idVal(ev::fromDouble(static_cast<double>(id)));
            const Value callArgs[3] = { actP.get(), pObj.get(), idVal.get() };
            auto res = ev::call(actFnP.get(), hookObj->get(), std::span<const Value>(callArgs, 3));
            if (!res.thrown && ev::toBool(res.value)) {
                return true;
            }
        }
    }

    auto tree = activeTree();
    if (tree) {
        auto n = tree->get_node(id);
        if (n) {
            auto customRoleAttr = n->get_attribute("custom_role");
            if (customRoleAttr) {
                std::lock_guard lock(g_custom_mu);
                auto itRole = g_custom_roles.find(*customRoleAttr);
                if (itRole != g_custom_roles.end()) {
                    roleHandler = itRole->second.action_handler;
                }
            }
        }
    }

    if (roleHandler && !ev::isUndefined(roleHandler->get()) && ev::isFunction(roleHandler->get())) {
        ObjectBuilder pb;
        pb.set("stringVal", params.string_val);
        pb.set("numberVal", params.number_val);
        pb.set("intVal", static_cast<double>(params.int_val));
        ev::Persistent actP(ev::fromUtf8(action));
        ev::Persistent pObj(pb.build());
        ev::Persistent idVal(ev::fromDouble(static_cast<double>(id)));
        const Value callArgs[3] = { actP.get(), pObj.get(), idVal.get() };
        auto res = ev::call(roleHandler->get(), ev::undefined(), std::span<const Value>(callArgs, 3));
        if (!res.thrown) {
            return ev::toBool(res.value);
        }
    }

    return false;
}

void installCustomOnto(Value a11yObj) {
    ObjectBuilder a11y(a11yObj);

    // bro.a11y.registerCustomRole(name, definition) -> boolean
    a11y.def("registerCustomRole", 2, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0)) return ev::fromBool(false);
        std::string name = reader.getString(0);
        if (name.empty()) return ev::fromBool(false);

        CustomRole role;
        role.name = name;

        if (reader.isObject(1)) {
            const auto& def = reader.getPersistent(1);
            role.base_role = ArgReader::getPropString(def, "baseRole", "canvas");
            role.description = ArgReader::getPropString(def, "description");

            if (ArgReader::hasProp(def, "actionHandler")) {
                ev::Persistent h(ArgReader::getProp(def, "actionHandler"));
                if (ev::isFunction(h.get())) {
                    role.action_handler = std::make_shared<ev::Persistent>(h.get());
                }
            }

            if (ArgReader::hasProp(def, "supportedActions")) {
                ev::Persistent acts(ArgReader::getProp(def, "supportedActions"));
                if (ev::isObject(acts.get())) {
                    double len = ArgReader::getPropDouble(acts, "length", 0.0);
                    for (uint32_t i = 0; i < static_cast<uint32_t>(len); ++i) {
                        ev::Persistent elem(ev::getElement(acts.get(), i));
                        if (ev::isString(elem.get())) {
                            role.supported_actions.push_back(ev::toUtf8(elem.get()));
                        }
                    }
                }
            }

            if (ArgReader::hasProp(def, "supportedStates")) {
                ev::Persistent sts(ArgReader::getProp(def, "supportedStates"));
                if (ev::isObject(sts.get())) {
                    double len = ArgReader::getPropDouble(sts, "length", 0.0);
                    for (uint32_t i = 0; i < static_cast<uint32_t>(len); ++i) {
                        ev::Persistent elem(ev::getElement(sts.get(), i));
                        if (ev::isString(elem.get())) {
                            role.supported_states.push_back(ev::toUtf8(elem.get()));
                        }
                    }
                }
            }
        }

        std::lock_guard lock(g_custom_mu);
        g_custom_roles[name] = std::move(role);
        return ev::fromBool(true);
    });

    // bro.a11y.unregisterCustomRole(name) -> boolean
    a11y.def("unregisterCustomRole", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0)) return ev::fromBool(false);
        std::string name = reader.getString(0);
        std::lock_guard lock(g_custom_mu);
        return ev::fromBool(g_custom_roles.erase(name) > 0);
    });

    // bro.a11y.hasCustomRole(name) -> boolean
    a11y.def("hasCustomRole", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0)) return ev::fromBool(false);
        std::string name = reader.getString(0);
        std::lock_guard lock(g_custom_mu);
        return ev::fromBool(g_custom_roles.find(name) != g_custom_roles.end());
    });

    // bro.a11y.getCustomRoles() -> Array
    a11y.def("getCustomRoles", 0, [](Value, std::span<const Value>) -> Value {
        std::vector<CustomRole> roles;
        {
            std::lock_guard lock(g_custom_mu);
            roles.reserve(g_custom_roles.size());
            for (const auto& [_, r] : g_custom_roles) {
                roles.push_back(r);
            }
        }

        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(roles.size())));
        for (uint32_t i = 0; i < roles.size(); ++i) {
            ObjectBuilder b;
            b.set("name", roles[i].name);
            b.set("baseRole", roles[i].base_role);
            b.set("description", roles[i].description);

            ev::Persistent acts(ev::makeArray(static_cast<uint32_t>(roles[i].supported_actions.size())));
            for (uint32_t j = 0; j < roles[i].supported_actions.size(); ++j) {
                ev::Persistent s(ev::fromUtf8(roles[i].supported_actions[j]));
                acts.set(ev::setElement(acts.get(), j, s.get()));
            }
            b.set("supportedActions", acts.get());

            ev::Persistent sts(ev::makeArray(static_cast<uint32_t>(roles[i].supported_states.size())));
            for (uint32_t j = 0; j < roles[i].supported_states.size(); ++j) {
                ev::Persistent s(ev::fromUtf8(roles[i].supported_states[j]));
                sts.set(ev::setElement(sts.get(), j, s.get()));
            }
            b.set("supportedStates", sts.get());

            ev::Persistent item(b.build());
            arr.set(ev::setElement(arr.get(), i, item.get()));
        }
        return arr.get();
    });

    // bro.a11y.registerWidgetHook(nodeId, hooks) -> boolean
    a11y.def("registerWidgetHook", 2, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isNumber(0) || !reader.isObject(1)) return ev::fromBool(false);
        uint64_t id = reader.getUint64(0);

        WidgetHook hook;
        hook.node_id = id;
        hook.hooks = std::make_shared<ev::Persistent>(reader.get(1));

        {
            std::lock_guard lock(g_custom_mu);
            g_widget_hooks[id] = std::move(hook);
        }

        auto tree = activeTree();
        if (tree) {
            auto n = tree->get_node(id);
            if (n) {
                n->set_action_handler([](NodeId nid, std::string_view act, const ActionParams& params) -> bool {
                    return dispatchWidgetAction(nid, act, params);
                });
            }
        }

        return ev::fromBool(true);
    });

    // bro.a11y.unregisterWidgetHook(nodeId) -> boolean
    a11y.def("unregisterWidgetHook", 1, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isNumber(0)) return ev::fromBool(false);
        uint64_t id = reader.getUint64(0);
        std::lock_guard lock(g_custom_mu);
        return ev::fromBool(g_widget_hooks.erase(id) > 0);
    });
}

} // namespace broa11y::api
