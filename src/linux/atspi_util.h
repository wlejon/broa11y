// Helpers shared by the AT-SPI interface handlers.
#pragma once

#include "linux/atspi_server.h"

#include <cerrno>
#include <cmath>
#include <string_view>

namespace broa11y::atspi {

inline Server* srv(void* userdata) { return static_cast<Server*>(userdata); }

inline const char* path_of_msg(sd_bus_message* m) { return sd_bus_message_get_path(m); }

// Resolves an object path; sets an UnknownObject error when it is gone.
inline bool target_of(Server* s, const char* path, Target& t, sd_bus_error* err) {
    if (s->resolve(path, t)) return true;
    sd_bus_error_setf(err, SD_BUS_ERROR_UNKNOWN_OBJECT, "no accessible at %s", path ? path : "(null)");
    return false;
}

// The node behind an object path; null (error set) for the application
// object or a vanished node.
inline Node* node_at(Server* s, const char* path, sd_bus_error* err) {
    Target t;
    if (!target_of(s, path, t, err)) return nullptr;
    if (t.app) {
        sd_bus_error_setf(err, SD_BUS_ERROR_UNKNOWN_INTERFACE, "the application object has no such interface");
        return nullptr;
    }
    return s->tree->get_node(t.id);
}

// A method return built up piece by piece; send() sends it if building went well.
struct Reply {
    sd_bus_message* msg = nullptr;
    int r = 0;
    explicit Reply(sd_bus_message* call) { r = sd_bus_message_new_method_return(call, &msg); }
    ~Reply() { sd_bus_message_unref(msg); }
    Reply(const Reply&) = delete;
    Reply& operator=(const Reply&) = delete;
    int send() { return r < 0 ? r : sd_bus_send(nullptr, msg, nullptr); }
};

inline int reply_bool(sd_bus_message* m, bool v) { return sd_bus_reply_method_return(m, "b", v ? 1 : 0); }

inline int32_t ri(double v) { return static_cast<int32_t>(std::lround(v)); }

// A node's bounds in an AtspiCoordType (0 screen, 1 window, 2 parent); the
// model's bounds are window-relative.
inline RectF rect_in(const Server* s, const Node* n, uint32_t coord_type) {
    RectF b = n->bounds();
    if (coord_type == 0) {
        PointF o = s->window_origin();
        b.x += o.x;
        b.y += o.y;
    } else if (coord_type == 2) {
        if (const Node* p = n->parent()) {
            b.x -= p->bounds().x;
            b.y -= p->bounds().y;
        }
    }
    return b;
}

// A point in an AtspiCoordType, as window coordinates.
inline PointF to_window(const Server* s, const Node* n, int32_t x, int32_t y, uint32_t coord_type) {
    PointF p{static_cast<double>(x), static_cast<double>(y)};
    if (coord_type == 0) {
        PointF o = s->window_origin();
        p.x -= o.x;
        p.y -= o.y;
    } else if (coord_type == 2) {
        if (const Node* parent = n->parent()) {
            p.x += parent->bounds().x;
            p.y += parent->bounds().y;
        }
    }
    return p;
}

} // namespace broa11y::atspi
