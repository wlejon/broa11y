// AT-SPI 2 over sd-bus: the state LinuxBridge shares with the D-Bus object
// handlers.
//
// Object layout, as GTK and Qt lay it out:
//   /org/a11y/atspi/accessible/root   the application (role APPLICATION);
//                                     its one child is the tree's root node
//   /org/a11y/atspi/accessible/<id>   every node of the tree, by NodeId
//   /org/a11y/atspi/cache             org.a11y.atspi.Cache
//
// Offsets on the wire are characters (code points); the model's are UTF-8
// bytes, converted through text::TextIndex.
#pragma once

#include "broa11y/linux_bridge.h"
#include "broa11y/tree.h"

#include <systemd/sd-bus.h>

#include <functional>
#include <string>
#include <vector>

namespace broa11y::atspi {

inline constexpr const char* kPathPrefix = "/org/a11y/atspi/accessible";
inline constexpr const char* kAppPath = "/org/a11y/atspi/accessible/root";
inline constexpr const char* kNullPath = "/org/a11y/atspi/null";
inline constexpr const char* kCachePath = "/org/a11y/atspi/cache";

inline constexpr const char* kIfaceAccessible = "org.a11y.atspi.Accessible";
inline constexpr const char* kIfaceApplication = "org.a11y.atspi.Application";
inline constexpr const char* kIfaceComponent = "org.a11y.atspi.Component";
inline constexpr const char* kIfaceAction = "org.a11y.atspi.Action";
inline constexpr const char* kIfaceText = "org.a11y.atspi.Text";
inline constexpr const char* kIfaceEditableText = "org.a11y.atspi.EditableText";
inline constexpr const char* kIfaceValue = "org.a11y.atspi.Value";
inline constexpr const char* kIfaceCache = "org.a11y.atspi.Cache";
inline constexpr const char* kIfaceEventObject = "org.a11y.atspi.Event.Object";
inline constexpr const char* kIfaceEventWindow = "org.a11y.atspi.Event.Window";

enum Iface : unsigned {
    kAccessible = 1u << 0,
    kApplication = 1u << 1,
    kComponent = 1u << 2,
    kAction = 1u << 3,
    kText = 1u << 4,
    kEditableText = 1u << 5,
    kValue = 1u << 6,
};

// An object on the bus: the application object or one node.
struct Target {
    bool app = false;
    NodeId id = kInvalidNodeId;
};

struct Server {
    LinuxBridgeConfig config;
    Tree* tree = nullptr;
    sd_bus* bus = nullptr;
    std::string unique_name;
    // The registry's desktop object, the application object's parent once
    // Socket.Embed has answered.
    std::string desktop_bus;
    std::string desktop_path = kNullPath;
    int32_t app_id = 0;  // assigned by the registry through Application.Id
    std::vector<sd_bus_slot*> slots;

    [[nodiscard]] std::string path_of(NodeId id) const;
    // A path under kPathPrefix naming the application or an existing node.
    [[nodiscard]] bool resolve(const char* path, Target& out) const;
    [[nodiscard]] unsigned interfaces_of(const Target& t) const;
    [[nodiscard]] PointF window_origin() const;
    // The AtspiStateSet bitmask (two 32-bit words) the object reports.
    [[nodiscard]] std::pair<uint32_t, uint32_t> states_of(const Target& t) const;

    // Appends an object reference (so); a missing node becomes the null ref.
    int append_ref(sd_bus_message* m, NodeId id) const;
    int append_app_ref(sd_bus_message* m) const;
    // Appends one Cache item: ((so)(so)(so)iiassusau).
    int append_cache_item(sd_bus_message* m, const Target& t) const;

    // Emits an AT-SPI event signal (siiva{sv}) from a node's object;
    // `any` appends the variant's content type and value.
    int emit(NodeId id, const char* iface, const char* member, const std::string& detail, int32_t d1, int32_t d2,
             const std::function<int(sd_bus_message*)>& any) const;
};

// Registers the object vtables, the cache object and the node enumerator.
int register_objects(Server& s);

// The AT-SPI state name for a model state ("focused", "read-only", ...).
const char* state_name(State s);

} // namespace broa11y::atspi
