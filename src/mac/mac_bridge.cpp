#include "broa11y/mac_bridge.h"
#include "mac_accessible_element.h"
#include "mac_constants.h"
#include "mac_types.h"
#include "broa11y/tree.h"

#include <unordered_map>

namespace broa11y {

class MacBridge::Impl {
public:
    explicit Impl(MacBridgeConfig config) : config_(std::move(config)) {}

    ~Impl() {
        shutdown();
    }

    bool initialize(Tree* tree) {
        tree_ = tree;
        active_ = true;

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
        tree_ = nullptr;
        active_ = false;
        elements_.clear();
    }

    void handle_event(const Event& event) {
        if (!tree_) return;

        switch (event.type) {
            case EventType::PropertyChanged: {
                if (const auto* p = event.get_if<PropertyChangedPayload>()) {
                    std::string notif_name = std::string(mac::kValueChangedNotification);
                    if (p->property_name == "name") {
                        notif_name = std::string(mac::kTitleChangedNotification);
                    }
                    emitted_notifications_.push_back(mac::MacNotification{
                        .name = notif_name,
                        .node_id = event.node_id,
                        .user_info = {{"property", p->property_name}, {"value", p->new_value}}
                    });
                }
                break;
            }
            case EventType::StateChanged: {
                if (const auto* p = event.get_if<StateChangedPayload>()) {
                    if (p->state == State::Focused && p->enabled) {
                        emitted_notifications_.push_back(mac::MacNotification{
                            .name = std::string(mac::kFocusedUIElementChangedNotification),
                            .node_id = event.node_id,
                            .user_info = {}
                        });
                    }
                }
                break;
            }
            case EventType::NodeRemoved: {
                emitted_notifications_.push_back(mac::MacNotification{
                    .name = std::string(mac::kUIElementDestroyedNotification),
                    .node_id = event.node_id,
                    .user_info = {}
                });
                break;
            }
            case EventType::ChildrenChanged: {
                emitted_notifications_.push_back(mac::MacNotification{
                    .name = std::string(mac::kSelectedChildrenChangedNotification),
                    .node_id = event.node_id,
                    .user_info = {}
                });
                break;
            }
            case EventType::TextSelectionChanged: {
                emitted_notifications_.push_back(mac::MacNotification{
                    .name = std::string(mac::kSelectedTextChangedNotification),
                    .node_id = event.node_id,
                    .user_info = {}
                });
                break;
            }
            case EventType::Announcement: {
                if (const auto* p = event.get_if<AnnouncementPayload>()) {
                    emitted_notifications_.push_back(mac::MacNotification{
                        .name = std::string(mac::kAnnouncementRequestedNotification),
                        .node_id = event.node_id,
                        .user_info = {{"announcement", p->message}}
                    });
                }
                break;
            }
            default:
                break;
        }
    }

    void process_events() {}

    [[nodiscard]] bool is_active() const noexcept { return active_; }

    [[nodiscard]] size_t emitted_notification_count() const noexcept {
        return emitted_notifications_.size();
    }

    [[nodiscard]] std::vector<std::string> get_emitted_notifications() const {
        std::vector<std::string> res;
        for (const auto& n : emitted_notifications_) {
            res.push_back(n.name);
        }
        return res;
    }

    void clear_emitted_notifications() {
        emitted_notifications_.clear();
    }

    mac::MacAccessibleElement* get_or_create_element(NodeId id) {
        if (!tree_) return nullptr;
        Node* node = tree_->get_node(id);
        if (!node) return nullptr;

        auto it = elements_.find(id);
        if (it != elements_.end()) {
            return it->second.get();
        }
        auto el = std::make_unique<mac::MacAccessibleElement>(node);
        auto* ptr = el.get();
        elements_[id] = std::move(el);
        return ptr;
    }

    std::string query_element_attribute(NodeId node_id, std::string_view attribute) {
        auto* el = get_or_create_element(node_id);
        return el ? el->get_attribute(attribute) : "";
    }

    std::string query_parameterized_attribute(NodeId node_id,
                                             std::string_view attribute,
                                             std::string_view parameter) {
        auto* el = get_or_create_element(node_id);
        return el ? el->get_parameterized_attribute(attribute, parameter) : "";
    }

    bool perform_element_action(NodeId node_id, std::string_view action) {
        auto* el = get_or_create_element(node_id);
        return el ? el->perform_action(action) : false;
    }

private:
    MacBridgeConfig config_;
    bool active_ = false;
    Tree* tree_ = nullptr;
    EventListenerId listener_id_ = 0;
    std::unordered_map<NodeId, std::unique_ptr<mac::MacAccessibleElement>> elements_;
    std::vector<mac::MacNotification> emitted_notifications_;
};

MacBridge::MacBridge(MacBridgeConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

MacBridge::~MacBridge() = default;

bool MacBridge::initialize(Tree* tree) {
    return impl_->initialize(tree);
}

void MacBridge::shutdown() {
    impl_->shutdown();
}

void MacBridge::handle_event(const Event& event) {
    impl_->handle_event(event);
}

void MacBridge::process_events() {
    impl_->process_events();
}

bool MacBridge::is_active() const noexcept {
    return impl_->is_active();
}

size_t MacBridge::emitted_notification_count() const noexcept {
    return impl_->emitted_notification_count();
}

std::vector<std::string> MacBridge::get_emitted_notifications() const {
    return impl_->get_emitted_notifications();
}

void MacBridge::clear_emitted_notifications() {
    impl_->clear_emitted_notifications();
}

std::string MacBridge::query_element_attribute(NodeId node_id, std::string_view attribute) const {
    return impl_->query_element_attribute(node_id, attribute);
}

std::string MacBridge::query_parameterized_attribute(NodeId node_id,
                                                    std::string_view attribute,
                                                    std::string_view parameter) const {
    return impl_->query_parameterized_attribute(node_id, attribute, parameter);
}

bool MacBridge::perform_element_action(NodeId node_id, std::string_view action) {
    return impl_->perform_element_action(node_id, action);
}

std::unique_ptr<Bridge> create_mac_bridge(Tree* tree) {
    auto bridge = std::make_unique<MacBridge>();
    if (tree) {
        bridge->initialize(tree);
    }
    return bridge;
}

} // namespace broa11y
