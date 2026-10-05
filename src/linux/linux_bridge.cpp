#include "broa11y/linux_bridge.h"
#include "atspi_constants.h"
#include "atspi_node_adaptor.h"
#include "atspi_serializer.h"
#include "dbus_connection.h"
#include "broa11y/tree.h"

namespace broa11y {

class LinuxBridge::Impl {
public:
    explicit Impl(LinuxBridgeConfig config)
        : config_(std::move(config)),
          conn_() {}

    ~Impl() {
        shutdown();
    }

    bool initialize(Tree* tree) {
        tree_ = tree;
        if (!conn_.connect()) {
            return false;
        }

        if (tree_) {
            listener_id_ = tree_->add_listener([this](const Event& ev) {
                handle_event(ev);
            });
        }
        return true;
    }

    void shutdown() {
        if (tree_ && listener_id_ != 0) {
            tree_->remove_listener(listener_id_);
            listener_id_ = 0;
        }
        conn_.disconnect();
        tree_ = nullptr;
    }

    void handle_event(const Event& event) {
        if (!tree_) return;

        std::string path = atspi::NodeAdaptor::node_id_to_path(event.node_id, tree_->root_id());

        switch (event.type) {
            case EventType::StateChanged: {
                if (const auto* p = event.get_if<StateChangedPayload>()) {
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "StateChanged",
                        .path = path,
                        .detail = std::string(state_to_string(p->state)),
                        .detail1 = p->enabled ? 1 : 0,
                        .detail2 = 0
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            case EventType::PropertyChanged: {
                if (const auto* p = event.get_if<PropertyChangedPayload>()) {
                    std::string detail = "AccessibleName";
                    if (p->property_name == "name") {
                        detail = "AccessibleName";
                    } else if (p->property_name == "description") {
                        detail = "AccessibleDescription";
                    } else if (p->property_name == "role") {
                        detail = "AccessibleRole";
                    }
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "PropertyChange",
                        .path = path,
                        .detail = detail,
                        .detail1 = 0,
                        .detail2 = 0
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            case EventType::CaretMoved: {
                if (const auto* p = event.get_if<CaretMovedPayload>()) {
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "TextCaretMoved",
                        .path = path,
                        .detail = "",
                        .detail1 = p->new_offset,
                        .detail2 = 0
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            case EventType::TextSelectionChanged: {
                if (const auto* p = event.get_if<TextSelectionPayload>()) {
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "TextSelectionChanged",
                        .path = path,
                        .detail = "",
                        .detail1 = p->selection.start_offset,
                        .detail2 = p->selection.end_offset
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            case EventType::ChildrenChanged: {
                if (const auto* p = event.get_if<ChildrenChangedPayload>()) {
                    std::string detail = (p->change_type == ChildrenChangeType::ChildAdded) ? "add" : "remove";
                    atspi::Reference child_ref = atspi::NodeAdaptor::make_reference(
                        conn_.unique_name(), p->child_id, tree_->root_id()
                    );
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "ChildrenChanged",
                        .path = path,
                        .detail = detail,
                        .detail1 = static_cast<int32_t>(p->index),
                        .detail2 = 0,
                        .any_data = atspi::Serializer::encode_reference(child_ref)
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            case EventType::WindowActivated:
            case EventType::WindowDeactivated: {
                std::string member = (event.type == EventType::WindowActivated) ? "Activate" : "Deactivate";
                atspi::Signal sig{
                    .interface_name = std::string(atspi::kDbusInterfaceEventWindow),
                    .member = member,
                    .path = path,
                    .detail = "",
                    .detail1 = 0,
                    .detail2 = 0
                };
                conn_.emit_signal(sig);
                break;
            }
            case EventType::Announcement: {
                if (const auto* p = event.get_if<AnnouncementPayload>()) {
                    atspi::Signal sig{
                        .interface_name = std::string(atspi::kDbusInterfaceEventObject),
                        .member = "Announcement",
                        .path = path,
                        .detail = p->message,
                        .detail1 = static_cast<int32_t>(p->priority),
                        .detail2 = 0
                    };
                    conn_.emit_signal(sig);
                }
                break;
            }
            default:
                break;
        }
    }

    void process_events() {
        conn_.flush();
    }

    [[nodiscard]] bool is_active() const noexcept {
        return conn_.is_connected();
    }

    [[nodiscard]] size_t emitted_signal_count() const noexcept {
        return conn_.emitted_signals().size();
    }

    [[nodiscard]] std::vector<std::string> get_emitted_signal_names() const {
        std::vector<std::string> names;
        for (const auto& s : conn_.emitted_signals()) {
            std::string n = s.interface_name + ":" + s.member;
            if (!s.detail.empty()) {
                n += ":" + s.detail;
            }
            names.push_back(n);
        }
        return names;
    }

    void clear_emitted_signals() {
        conn_.clear_emitted_signals();
    }

    std::string handle_method_call(std::string_view path,
                                  std::string_view interface_name,
                                  std::string_view method_name,
                                  const std::vector<std::string>& args) {
        atspi::MethodCall call{
            .path = std::string(path),
            .interface_name = std::string(interface_name),
            .member = std::string(method_name),
            .args = args
        };
        atspi::MethodReply reply = atspi::NodeAdaptor::handle_call(tree_, conn_.unique_name(), call);
        return atspi::Serializer::serialize_reply(reply);
    }

private:
    LinuxBridgeConfig config_;
    atspi::DbusConnection conn_;
    Tree* tree_ = nullptr;
    EventListenerId listener_id_ = 0;
};

LinuxBridge::LinuxBridge(LinuxBridgeConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

LinuxBridge::~LinuxBridge() = default;

bool LinuxBridge::initialize(Tree* tree) {
    return impl_->initialize(tree);
}

void LinuxBridge::shutdown() {
    impl_->shutdown();
}

void LinuxBridge::handle_event(const Event& event) {
    impl_->handle_event(event);
}

void LinuxBridge::process_events() {
    impl_->process_events();
}

bool LinuxBridge::is_active() const noexcept {
    return impl_->is_active();
}

size_t LinuxBridge::emitted_signal_count() const noexcept {
    return impl_->emitted_signal_count();
}

std::vector<std::string> LinuxBridge::get_emitted_signal_names() const {
    return impl_->get_emitted_signal_names();
}

void LinuxBridge::clear_emitted_signals() {
    impl_->clear_emitted_signals();
}

std::string LinuxBridge::handle_method_call(std::string_view path,
                                           std::string_view interface_name,
                                           std::string_view method_name,
                                           const std::vector<std::string>& args) {
    return impl_->handle_method_call(path, interface_name, method_name, args);
}

std::unique_ptr<Bridge> create_linux_bridge(Tree* tree) {
    auto bridge = std::make_unique<LinuxBridge>();
    if (tree) {
        bridge->initialize(tree);
    }
    return bridge;
}

} // namespace broa11y
