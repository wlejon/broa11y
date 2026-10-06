#include "broa11y/win_bridge.h"
#include "common/text_util.h"
#include "win/uia_provider.h"

#include <commctrl.h>

namespace broa11y {

namespace {

constexpr UINT_PTR kSubclassId = 0xB70A11;

VARIANT bstr_variant(std::string_view s) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = uia::to_bstr(s);
    return v;
}

VARIANT bool_variant(bool b) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BOOL;
    v.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;
    return v;
}

VARIANT double_variant(double d) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_R8;
    v.dblVal = d;
    return v;
}

} // namespace

class WinBridge::Impl {
public:
    explicit Impl(WinBridgeConfig config) : config_(std::move(config)) {}

    ~Impl() { shutdown(); }

    bool initialize(Tree* tree) {
        if (active_) return true;
        error_.clear();
        if (!tree) return fail("no tree");
        HWND hwnd = static_cast<HWND>(config_.hwnd);
        if (!hwnd || !IsWindow(hwnd)) {
            return fail("WinBridgeConfig::hwnd is not a window; UI Automation finds providers through one");
        }
        if (GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) {
            return fail("initialize() must run on the thread that owns the window");
        }
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hr == RPC_E_CHANGED_MODE) {
            return fail("the window's thread is in a multithreaded COM apartment; UI Automation would call "
                        "providers on arbitrary threads, so the bridge needs a single-threaded apartment");
        }
        if (FAILED(hr)) return fail("CoInitializeEx failed");
        com_initialized_ = true;

        ctx_ = std::make_shared<uia::Context>();
        ctx_->tree = tree;
        ctx_->hwnd = hwnd;
        tree_ = tree;

        if (!SetWindowSubclass(hwnd, &Impl::subclass_proc, kSubclassId, reinterpret_cast<DWORD_PTR>(this))) {
            ctx_.reset();
            CoUninitialize();
            com_initialized_ = false;
            return fail("SetWindowSubclass failed on the host window");
        }
        listener_ = tree_->add_listener([this](const Event& ev) { handle_event(ev); });
        active_ = true;
        return true;
    }

    void shutdown() {
        if (!active_) return;
        active_ = false;
        if (tree_ && listener_) tree_->remove_listener(listener_);
        listener_ = 0;
        HWND hwnd = ctx_->hwnd;
        if (IsWindow(hwnd)) {
            RemoveWindowSubclass(hwnd, &Impl::subclass_proc, kSubclassId);
            // Tells UIA the window no longer serves a provider.
            UiaReturnRawElementProvider(hwnd, 0, 0, nullptr);
        }
        ctx_->tree = nullptr;
        ctx_->drop_all();
        ctx_.reset();
        tree_ = nullptr;
        if (com_initialized_) {
            CoUninitialize();
            com_initialized_ = false;
        }
    }

    void handle_event(const Event& ev);

    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    bool fail(std::string why) {
        error_ = std::move(why);
        return false;
    }

    static LRESULT CALLBACK subclass_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
        auto* self = reinterpret_cast<Impl*>(data);
        if (msg == WM_GETOBJECT && static_cast<long>(lp) == static_cast<long>(UiaRootObjectId) && self->ctx_ &&
            self->tree_) {
            uia::NodeProvider* root = self->ctx_->provider(self->tree_->root_id());
            if (root) {
                return UiaReturnRawElementProvider(hwnd, wp, lp, static_cast<IRawElementProviderSimple*>(root));
            }
        }
        if (msg == WM_NCDESTROY) {
            // The window is going away before the bridge: stop serving it.
            self->shutdown();
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    // A provider to raise an event on; null when nobody listens or the node is gone.
    IRawElementProviderSimple* target(NodeId id) {
        if (!ctx_ || !UiaClientsAreListening()) return nullptr;
        uia::NodeProvider* p = ctx_->provider(id);
        return p ? static_cast<IRawElementProviderSimple*>(p) : nullptr;
    }

    void raise_property(NodeId id, PROPERTYID prop, VARIANT old_v, VARIANT new_v) {
        if (IRawElementProviderSimple* p = target(id)) {
            UiaRaiseAutomationPropertyChangedEvent(p, prop, old_v, new_v);
        }
        VariantClear(&old_v);
        VariantClear(&new_v);
    }

    void raise(NodeId id, EVENTID event) {
        if (IRawElementProviderSimple* p = target(id)) UiaRaiseAutomationEvent(p, event);
    }

    friend class WinBridge;

    WinBridgeConfig config_;
    std::shared_ptr<uia::Context> ctx_;
    Tree* tree_ = nullptr;
    EventListenerId listener_ = 0;
    bool active_ = false;
    bool com_initialized_ = false;
    std::string error_;
};

void WinBridge::Impl::handle_event(const Event& ev) {
    if (!active_ || !ctx_) return;
    switch (ev.type) {
        case EventType::FocusChanged:
            if (ev.node_id != kInvalidNodeId) raise(ev.node_id, UIA_AutomationFocusChangedEventId);
            break;

        case EventType::PropertyChanged:
            if (const auto* p = ev.get_if<PropertyChangedPayload>()) {
                if (p->property_name == "name") {
                    raise_property(ev.node_id, UIA_NamePropertyId, bstr_variant(p->old_value),
                                   bstr_variant(p->new_value));
                } else if (p->property_name == "description") {
                    raise_property(ev.node_id, UIA_HelpTextPropertyId, bstr_variant(p->old_value),
                                   bstr_variant(p->new_value));
                } else if (p->property_name == "text") {
                    raise_property(ev.node_id, UIA_ValueValuePropertyId, bstr_variant(p->old_value),
                                   bstr_variant(p->new_value));
                    raise(ev.node_id, UIA_Text_TextChangedEventId);
                } else if (p->property_name == "role") {
                    VARIANT o;
                    VariantInit(&o);
                    VARIANT n;
                    VariantInit(&n);
                    n.vt = VT_I4;
                    n.lVal = static_cast<LONG>(role_to_uia_control_type(string_to_role(p->new_value)));
                    raise_property(ev.node_id, UIA_ControlTypePropertyId, o, n);
                }
            }
            break;

        case EventType::StateChanged:
            if (const auto* p = ev.get_if<StateChangedPayload>()) {
                switch (p->state) {
                    case State::Focused:
                        raise_property(ev.node_id, UIA_HasKeyboardFocusPropertyId, bool_variant(!p->enabled),
                                       bool_variant(p->enabled));
                        break;
                    case State::Disabled:
                        raise_property(ev.node_id, UIA_IsEnabledPropertyId, bool_variant(p->enabled),
                                       bool_variant(!p->enabled));
                        break;
                    case State::Checked:
                    case State::Indeterminate: {
                        Node* n = tree_->get_node(ev.node_id);
                        if (!n) break;
                        auto state_of = [&](bool checked, bool mixed) {
                            VARIANT v;
                            VariantInit(&v);
                            v.vt = VT_I4;
                            v.lVal = mixed ? ToggleState_Indeterminate : (checked ? ToggleState_On : ToggleState_Off);
                            return v;
                        };
                        bool checked = n->has_state(State::Checked);
                        bool mixed = n->has_state(State::Indeterminate);
                        bool old_checked = p->state == State::Checked ? !p->enabled : checked;
                        bool old_mixed = p->state == State::Indeterminate ? !p->enabled : mixed;
                        raise_property(ev.node_id, UIA_ToggleToggleStatePropertyId, state_of(old_checked, old_mixed),
                                       state_of(checked, mixed));
                        break;
                    }
                    case State::ReadOnly:
                        raise_property(ev.node_id, UIA_ValueIsReadOnlyPropertyId, bool_variant(!p->enabled),
                                       bool_variant(p->enabled));
                        break;
                    default:
                        break;
                }
            }
            break;

        case EventType::ValueChanged:
            if (const auto* p = ev.get_if<ValueChangedPayload>()) {
                raise_property(ev.node_id, UIA_RangeValueValuePropertyId, double_variant(p->old_value.current),
                               double_variant(p->new_value.current));
            }
            break;

        case EventType::BoundsChanged:
            if (const auto* p = ev.get_if<BoundsChangedPayload>()) {
                auto rect_variant = [&](const RectF& r) {
                    RECT s = uia::to_screen(*ctx_, r);
                    double v[4] = {static_cast<double>(s.left), static_cast<double>(s.top),
                                   static_cast<double>(s.right - s.left), static_cast<double>(s.bottom - s.top)};
                    VARIANT var;
                    VariantInit(&var);
                    var.vt = VT_ARRAY | VT_R8;
                    var.parray = SafeArrayCreateVector(VT_R8, 0, 4);
                    for (LONG i = 0; i < 4; ++i) SafeArrayPutElement(var.parray, &i, &v[i]);
                    return var;
                };
                raise_property(ev.node_id, UIA_BoundingRectanglePropertyId, rect_variant(p->old_bounds),
                               rect_variant(p->new_bounds));
            }
            break;

        case EventType::CaretMoved:
        case EventType::TextSelectionChanged:
            raise(ev.node_id, UIA_Text_TextSelectionChangedEventId);
            break;

        case EventType::ChildrenChanged:
            if (const auto* p = ev.get_if<ChildrenChangedPayload>()) {
                if (!UiaClientsAreListening()) break;
                if (p->change_type == ChildrenChangeType::ChildAdded) {
                    if (IRawElementProviderSimple* child = target(p->child_id)) {
                        int rid[3] = {UiaAppendRuntimeId, static_cast<int>(p->child_id & 0x7FFFFFFF),
                                      static_cast<int>(p->child_id >> 31)};
                        UiaRaiseStructureChangedEvent(child, StructureChangeType_ChildAdded, rid, 3);
                    }
                } else if (IRawElementProviderSimple* parent = target(ev.node_id)) {
                    int rid[3] = {UiaAppendRuntimeId, static_cast<int>(p->child_id & 0x7FFFFFFF),
                                  static_cast<int>(p->child_id >> 31)};
                    UiaRaiseStructureChangedEvent(parent, StructureChangeType_ChildRemoved, rid, 3);
                }
            }
            break;

        case EventType::NodeRemoved:
            ctx_->drop(ev.node_id);
            break;

        case EventType::Announcement:
            if (const auto* p = ev.get_if<AnnouncementPayload>()) {
                NodeId origin = tree_->contains_node(ev.node_id) ? ev.node_id : tree_->root_id();
                if (IRawElementProviderSimple* prov = target(origin)) {
                    BSTR msg = uia::to_bstr(p->message);
                    BSTR activity = SysAllocString(L"broa11y.announcement");
                    UiaRaiseNotificationEvent(prov, NotificationKind_Other,
                                              p->priority == AnnouncementPriority::Assertive
                                                  ? NotificationProcessing_ImportantMostRecent
                                                  : NotificationProcessing_MostRecent,
                                              msg, activity);
                    SysFreeString(msg);
                    SysFreeString(activity);
                }
            }
            break;

        case EventType::WindowActivated:
            raise(ev.node_id, UIA_Window_WindowOpenedEventId);
            break;

        case EventType::NodeAdded:
        case EventType::WindowDeactivated:
        case EventType::Count:
            break;
    }
}

WinBridge::WinBridge(WinBridgeConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

WinBridge::~WinBridge() = default;

bool WinBridge::initialize(Tree* tree) { return impl_->initialize(tree); }

void WinBridge::shutdown() { impl_->shutdown(); }

void WinBridge::handle_event(const Event& event) { impl_->handle_event(event); }

void WinBridge::process_events() {}

bool WinBridge::is_active() const noexcept { return impl_->active(); }

std::string WinBridge::last_error() const { return impl_->error(); }

} // namespace broa11y
