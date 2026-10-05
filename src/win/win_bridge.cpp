#include "broa11y/win_bridge.h"
#include "uia_constants.h"
#include "uia_node_provider.h"
#include "uia_types.h"
#include "broa11y/tree.h"

#include <unordered_map>

namespace broa11y {

class WinBridge::Impl {
public:
    explicit Impl(WinBridgeConfig config) : config_(std::move(config)) {}

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
        providers_.clear();
    }

    void handle_event(const Event& event) {
        if (!tree_) return;

        switch (event.type) {
            case EventType::PropertyChanged: {
                if (const auto* p = event.get_if<PropertyChangedPayload>()) {
                    int32_t prop_id = 0;
                    if (p->property_name == "name") prop_id = uia::kNamePropertyId;
                    else if (p->property_name == "description") prop_id = uia::kHelpTextPropertyId;
                    else if (p->property_name == "text") prop_id = uia::kValueValuePropertyId;

                    emitted_events_.push_back(uia::UiaEvent{
                        .event_id = uia::kAutomationPropertyChangedEventId,
                        .node_id = event.node_id,
                        .property_id = prop_id,
                        .old_value = p->old_value,
                        .new_value = p->new_value,
                        .description = "PropertyChange: " + p->property_name
                    });
                }
                break;
            }
            case EventType::StateChanged: {
                if (const auto* p = event.get_if<StateChangedPayload>()) {
                    int32_t prop_id = (p->state == State::Focused)
                        ? uia::kHasKeyboardFocusPropertyId : uia::kIsEnabledPropertyId;

                    emitted_events_.push_back(uia::UiaEvent{
                        .event_id = uia::kAutomationPropertyChangedEventId,
                        .node_id = event.node_id,
                        .property_id = prop_id,
                        .new_value = p->enabled ? "true" : "false",
                        .description = "StateChange: " + std::string(state_to_string(p->state))
                    });
                }
                break;
            }
            case EventType::ChildrenChanged: {
                emitted_events_.push_back(uia::UiaEvent{
                    .event_id = uia::kStructureChangedEventId,
                    .node_id = event.node_id,
                    .description = "StructureChanged"
                });
                break;
            }
            case EventType::TextSelectionChanged: {
                emitted_events_.push_back(uia::UiaEvent{
                    .event_id = uia::kText_TextSelectionChangedEventId,
                    .node_id = event.node_id,
                    .description = "TextSelectionChanged"
                });
                break;
            }
            case EventType::Announcement: {
                if (const auto* p = event.get_if<AnnouncementPayload>()) {
                    emitted_events_.push_back(uia::UiaEvent{
                        .event_id = uia::kNotificationEventId,
                        .node_id = event.node_id,
                        .description = p->message
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

    [[nodiscard]] size_t emitted_event_count() const noexcept {
        return emitted_events_.size();
    }

    [[nodiscard]] std::vector<std::string> get_emitted_event_names() const {
        std::vector<std::string> names;
        for (const auto& ev : emitted_events_) {
            names.push_back(std::to_string(ev.event_id) + ":" + ev.description);
        }
        return names;
    }

    void clear_emitted_events() {
        emitted_events_.clear();
    }

    uia::UiaNodeProvider* get_or_create_provider(NodeId id) {
        if (!tree_) return nullptr;
        Node* node = tree_->get_node(id);
        if (!node) return nullptr;

        auto it = providers_.find(id);
        if (it != providers_.end()) {
            return it->second.get();
        }
        auto provider = std::make_unique<uia::UiaNodeProvider>(node);
        auto* ptr = provider.get();
        providers_[id] = std::move(provider);
        return ptr;
    }

    std::string query_provider_property(NodeId node_id, int32_t property_id) {
        auto* prov = get_or_create_provider(node_id);
        return prov ? prov->get_property_value(property_id) : "";
    }

    bool execute_provider_action(NodeId node_id, int32_t pattern_id, std::string_view action_name) {
        auto* prov = get_or_create_provider(node_id);
        return prov ? prov->execute_action(pattern_id, action_name) : false;
    }

private:
    WinBridgeConfig config_;
    bool active_ = false;
    Tree* tree_ = nullptr;
    EventListenerId listener_id_ = 0;
    std::unordered_map<NodeId, std::unique_ptr<uia::UiaNodeProvider>> providers_;
    std::vector<uia::UiaEvent> emitted_events_;
};

WinBridge::WinBridge(WinBridgeConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

WinBridge::~WinBridge() = default;

bool WinBridge::initialize(Tree* tree) {
    return impl_->initialize(tree);
}

void WinBridge::shutdown() {
    impl_->shutdown();
}

void WinBridge::handle_event(const Event& event) {
    impl_->handle_event(event);
}

void WinBridge::process_events() {
    impl_->process_events();
}

bool WinBridge::is_active() const noexcept {
    return impl_->is_active();
}

size_t WinBridge::emitted_event_count() const noexcept {
    return impl_->emitted_event_count();
}

std::vector<std::string> WinBridge::get_emitted_event_names() const {
    return impl_->get_emitted_event_names();
}

void WinBridge::clear_emitted_events() {
    impl_->clear_emitted_events();
}

std::string WinBridge::query_provider_property(NodeId node_id, int32_t property_id) const {
    return impl_->query_provider_property(node_id, property_id);
}

bool WinBridge::execute_provider_action(NodeId node_id, int32_t pattern_id, std::string_view action_name) {
    return impl_->execute_provider_action(node_id, pattern_id, action_name);
}

std::unique_ptr<Bridge> create_win_bridge(Tree* tree) {
    auto bridge = std::make_unique<WinBridge>();
    if (tree) {
        bridge->initialize(tree);
    }
    return bridge;
}

} // namespace broa11y
