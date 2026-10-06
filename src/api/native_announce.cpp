#include "host_a11y_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <string>

namespace broa11y::api {

void installAnnounceOnto(Value a11yObj) {
    ObjectBuilder a11y(a11yObj);

    // bro.a11y.announce(text, options?) -> boolean
    a11y.def("announce", 2, [](Value, std::span<const Value> args) -> Value {
        ArgReader reader(args);
        if (!reader.isString(0)) return ev::fromBool(false);
        std::string text = reader.getString(0);
        if (text.empty()) return ev::fromBool(false);

        AnnouncementPriority priority = AnnouncementPriority::Polite;
        NodeId originId = kInvalidNodeId;

        if (reader.isObject(1)) {
            const auto& opt = reader.getPersistent(1);
            std::string prioStr = ArgReader::getPropString(opt, "priority", "polite");
            if (prioStr == "assertive") {
                priority = AnnouncementPriority::Assertive;
            }
            if (ArgReader::hasProp(opt, "originId")) {
                originId = ArgReader::getPropUint64(opt, "originId");
            } else if (ArgReader::hasProp(opt, "origin_id")) {
                originId = ArgReader::getPropUint64(opt, "origin_id");
            }
        } else if (reader.isString(1)) {
            std::string prioStr = reader.getString(1);
            if (prioStr == "assertive") {
                priority = AnnouncementPriority::Assertive;
            }
        }

        auto tree = activeTree();
        if (!tree) return ev::fromBool(false);
        tree->announce(text, priority, originId);
        return ev::fromBool(true);
    });
}

} // namespace broa11y::api
