#include "win/uia_provider.h"
#include "common/text_util.h"

#include <cmath>
#include <string>

namespace broa11y::uia {

// ── Context ──────────────────────────────────────────────────────────────────

NodeProvider* Context::provider(NodeId id) {
    if (!tree || !tree->get_node(id)) return nullptr;
    auto it = providers.find(id);
    if (it != providers.end()) return it->second;
    // The map's reference is the one the constructor starts with.
    auto* p = new NodeProvider(shared_from_this(), id);
    providers.emplace(id, p);
    return p;
}

void Context::drop(NodeId id) {
    auto it = providers.find(id);
    if (it == providers.end()) return;
    NodeProvider* p = it->second;
    providers.erase(it);
    UiaDisconnectProvider(static_cast<IRawElementProviderSimple*>(p));
    p->Release();
}

void Context::drop_all() {
    auto all = std::move(providers);
    providers.clear();
    for (auto& [id, p] : all) {
        UiaDisconnectProvider(static_cast<IRawElementProviderSimple*>(p));
        p->Release();
    }
}

RECT to_screen(const Context& ctx, const RectF& r) {
    POINT origin{0, 0};
    if (ctx.hwnd) ClientToScreen(ctx.hwnd, &origin);
    RECT out;
    out.left = origin.x + static_cast<LONG>(std::lround(r.x));
    out.top = origin.y + static_cast<LONG>(std::lround(r.y));
    out.right = out.left + static_cast<LONG>(std::lround(r.width));
    out.bottom = out.top + static_cast<LONG>(std::lround(r.height));
    return out;
}

PointF to_client(const Context& ctx, double x, double y) {
    POINT p{static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y))};
    if (ctx.hwnd) ScreenToClient(ctx.hwnd, &p);
    return PointF{static_cast<double>(p.x), static_cast<double>(p.y)};
}

BSTR to_bstr(std::string_view utf8) {
    std::u16string w = text::utf8_to_utf16(utf8);
    return SysAllocStringLen(reinterpret_cast<const OLECHAR*>(w.data()), static_cast<UINT>(w.size()));
}

std::string from_wide(const wchar_t* s) {
    if (!s) return {};
    return text::utf16_to_utf8(std::u16string_view(reinterpret_cast<const char16_t*>(s)));
}

// ── NodeProvider: identity ───────────────────────────────────────────────────

NodeProvider::NodeProvider(std::shared_ptr<Context> ctx, NodeId id) : ctx_(std::move(ctx)), id_(id) {}

Node* NodeProvider::node() const {
    if (!ctx_ || !ctx_->tree) return nullptr;
    return ctx_->tree->get_node(id_);
}

bool NodeProvider::is_root() const {
    return ctx_ && ctx_->tree && ctx_->tree->root_id() == id_;
}

IFACEMETHODIMP NodeProvider::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple)) {
        *out = static_cast<IRawElementProviderSimple*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragment)) {
        *out = static_cast<IRawElementProviderFragment*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragmentRoot)) {
        if (!is_root()) return E_NOINTERFACE;
        *out = static_cast<IRawElementProviderFragmentRoot*>(this);
    } else if (riid == __uuidof(IInvokeProvider)) {
        *out = static_cast<IInvokeProvider*>(this);
    } else if (riid == __uuidof(IToggleProvider)) {
        *out = static_cast<IToggleProvider*>(this);
    } else if (riid == __uuidof(IValueProvider)) {
        *out = static_cast<IValueProvider*>(this);
    } else if (riid == __uuidof(IRangeValueProvider)) {
        *out = static_cast<IRangeValueProvider*>(this);
    } else if (riid == __uuidof(ITextProvider)) {
        *out = static_cast<ITextProvider*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) NodeProvider::AddRef() {
    return ++refs_;
}

IFACEMETHODIMP_(ULONG) NodeProvider::Release() {
    ULONG n = --refs_;
    if (n == 0) delete this;
    return n;
}

// ── IRawElementProviderSimple ────────────────────────────────────────────────

IFACEMETHODIMP NodeProvider::get_ProviderOptions(ProviderOptions* out) {
    if (!out) return E_POINTER;
    *out = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
}

bool NodeProvider::supports(PATTERNID pattern) const {
    Node* n = node();
    if (!n) return false;
    const Role role = n->role();
    auto has_action = [&](std::string_view a) {
        for (const auto& d : n->actions()) {
            if (d.name == a) return true;
        }
        return false;
    };
    switch (pattern) {
        case UIA_InvokePatternId:
            // Check boxes toggle rather than invoke.
            return role != Role::CheckBox && has_action(kActionActivate);
        case UIA_TogglePatternId:
            return role == Role::CheckBox;
        case UIA_ValuePatternId:
            return role == Role::TextInput || role == Role::ComboBox;
        case UIA_RangeValuePatternId:
            return n->value().has_value();
        case UIA_TextPatternId:
            return role == Role::TextInput || role == Role::Terminal || role == Role::Document;
        default:
            return false;
    }
}

IFACEMETHODIMP NodeProvider::GetPatternProvider(PATTERNID pattern, IUnknown** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    if (!supports(pattern)) return S_OK;
    switch (pattern) {
        case UIA_InvokePatternId: *out = static_cast<IInvokeProvider*>(this); break;
        case UIA_TogglePatternId: *out = static_cast<IToggleProvider*>(this); break;
        case UIA_ValuePatternId: *out = static_cast<IValueProvider*>(this); break;
        case UIA_RangeValuePatternId: *out = static_cast<IRangeValueProvider*>(this); break;
        case UIA_TextPatternId: *out = static_cast<ITextProvider*>(this); break;
        default: return S_OK;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP NodeProvider::GetPropertyValue(PROPERTYID property, VARIANT* out) {
    if (!out) return E_POINTER;
    VariantInit(out);
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;

    auto put_bool = [&](bool v) {
        out->vt = VT_BOOL;
        out->boolVal = v ? VARIANT_TRUE : VARIANT_FALSE;
    };
    auto put_str = [&](std::string_view v) {
        out->vt = VT_BSTR;
        out->bstrVal = to_bstr(v);
    };

    switch (property) {
        case UIA_ControlTypePropertyId:
            out->vt = VT_I4;
            out->lVal = static_cast<LONG>(role_to_uia_control_type(n->role()));
            break;
        case UIA_NamePropertyId:
            put_str(n->name());
            break;
        case UIA_HelpTextPropertyId:
            if (!n->description().empty()) put_str(n->description());
            break;
        case UIA_AutomationIdPropertyId:
            put_str(std::to_string(id_));
            break;
        case UIA_FrameworkIdPropertyId:
            put_str("broa11y");
            break;
        case UIA_ClassNamePropertyId:
            put_str(role_to_string(n->role()));
            break;
        case UIA_IsEnabledPropertyId:
            put_bool(!n->has_state(State::Disabled));
            break;
        case UIA_IsKeyboardFocusablePropertyId:
            put_bool(n->has_state(State::Focusable));
            break;
        case UIA_HasKeyboardFocusPropertyId:
            put_bool(ctx_->tree->focused_node_id() == id_);
            break;
        case UIA_IsOffscreenPropertyId:
            put_bool(n->bounds().is_empty());
            break;
        case UIA_IsPasswordPropertyId:
            put_bool(false);
            break;
        case UIA_IsRequiredForFormPropertyId:
            put_bool(n->has_state(State::Required));
            break;
        case UIA_ProcessIdPropertyId:
            out->vt = VT_I4;
            out->lVal = static_cast<LONG>(GetCurrentProcessId());
            break;
        case UIA_IsControlElementPropertyId:
        case UIA_IsContentElementPropertyId:
            put_bool(true);
            break;
        default:
            break;  // VT_EMPTY: UIA uses the host's value or its default
    }
    return S_OK;
}

IFACEMETHODIMP NodeProvider::get_HostRawElementProvider(IRawElementProviderSimple** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    // The root is hosted by the window: UIA merges in the HWND's own
    // properties and places it in the desktop tree. Other nodes have no host.
    if (is_root() && ctx_->hwnd) return UiaHostProviderFromHwnd(ctx_->hwnd, out);
    return S_OK;
}

// ── IRawElementProviderFragment ──────────────────────────────────────────────

IFACEMETHODIMP NodeProvider::Navigate(NavigateDirection direction, IRawElementProviderFragment** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;

    Node* target = nullptr;
    switch (direction) {
        case NavigateDirection_Parent:
            // The root's parent is the host window, which UIA supplies.
            if (!is_root()) target = n->parent();
            break;
        case NavigateDirection_NextSibling:
            if (!is_root()) target = n->next_sibling();
            break;
        case NavigateDirection_PreviousSibling:
            if (!is_root()) target = n->previous_sibling();
            break;
        case NavigateDirection_FirstChild:
            target = n->first_child();
            break;
        case NavigateDirection_LastChild:
            target = n->last_child();
            break;
    }
    if (!target) return S_OK;
    if (NodeProvider* p = ctx_->provider(target->id())) {
        p->AddRef();
        *out = static_cast<IRawElementProviderFragment*>(p);
    }
    return S_OK;
}

IFACEMETHODIMP NodeProvider::GetRuntimeId(SAFEARRAY** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    if (is_root()) return S_OK;  // the host window's runtime id stands for the root

    int ids[3] = {UiaAppendRuntimeId, static_cast<int>(id_ & 0x7FFFFFFF), static_cast<int>(id_ >> 31)};
    SAFEARRAY* sa = SafeArrayCreateVector(VT_I4, 0, 3);
    if (!sa) return E_OUTOFMEMORY;
    for (LONG i = 0; i < 3; ++i) SafeArrayPutElement(sa, &i, &ids[i]);
    *out = sa;
    return S_OK;
}

IFACEMETHODIMP NodeProvider::get_BoundingRectangle(UiaRect* out) {
    if (!out) return E_POINTER;
    *out = UiaRect{0, 0, 0, 0};
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (is_root()) return S_OK;  // the window's rectangle stands for the root
    RECT r = to_screen(*ctx_, n->bounds());
    *out = UiaRect{static_cast<double>(r.left), static_cast<double>(r.top),
                   static_cast<double>(r.right - r.left), static_cast<double>(r.bottom - r.top)};
    return S_OK;
}

IFACEMETHODIMP NodeProvider::GetEmbeddedFragmentRoots(SAFEARRAY** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    return S_OK;
}

IFACEMETHODIMP NodeProvider::SetFocus() {
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    return n->perform_action(kActionFocus) ? S_OK : UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP NodeProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    if (NodeProvider* root = ctx_->provider(ctx_->tree->root_id())) {
        root->AddRef();
        *out = static_cast<IRawElementProviderFragmentRoot*>(root);
    }
    return S_OK;
}

// ── IRawElementProviderFragmentRoot ──────────────────────────────────────────

IFACEMETHODIMP NodeProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    NodeId hit = ctx_->tree->hit_test(to_client(*ctx_, x, y));
    if (hit == kInvalidNodeId) return S_OK;
    if (NodeProvider* p = ctx_->provider(hit)) {
        p->AddRef();
        *out = static_cast<IRawElementProviderFragment*>(p);
    }
    return S_OK;
}

IFACEMETHODIMP NodeProvider::GetFocus(IRawElementProviderFragment** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    NodeId f = ctx_->tree->focused_node_id();
    if (f == kInvalidNodeId || f == ctx_->tree->root_id()) return S_OK;
    if (NodeProvider* p = ctx_->provider(f)) {
        p->AddRef();
        *out = static_cast<IRawElementProviderFragment*>(p);
    }
    return S_OK;
}

// ── Invoke / Toggle ──────────────────────────────────────────────────────────

IFACEMETHODIMP NodeProvider::Invoke() {
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (n->has_state(State::Disabled)) return UIA_E_ELEMENTNOTENABLED;
    if (!n->perform_action(kActionActivate)) return UIA_E_INVALIDOPERATION;
    // The node may be gone now: the handler owns the tree.
    if (node() && UiaClientsAreListening()) {
        UiaRaiseAutomationEvent(static_cast<IRawElementProviderSimple*>(this), UIA_Invoke_InvokedEventId);
    }
    return S_OK;
}

IFACEMETHODIMP NodeProvider::Toggle() {
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (n->has_state(State::Disabled)) return UIA_E_ELEMENTNOTENABLED;
    return n->perform_action(kActionActivate) ? S_OK : UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP NodeProvider::get_ToggleState(ToggleState* out) {
    if (!out) return E_POINTER;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (n->has_state(State::Indeterminate)) {
        *out = ToggleState_Indeterminate;
    } else {
        *out = n->has_state(State::Checked) ? ToggleState_On : ToggleState_Off;
    }
    return S_OK;
}

// ── Value / RangeValue ───────────────────────────────────────────────────────

IFACEMETHODIMP NodeProvider::SetValue(LPCWSTR value) {
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (n->has_state(State::Disabled)) return UIA_E_ELEMENTNOTENABLED;
    if (n->has_state(State::ReadOnly)) return UIA_E_INVALIDOPERATION;
    ActionParams params;
    params.string_val = from_wide(value);
    return n->perform_action(kActionSetValue, params) ? S_OK : UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP NodeProvider::get_Value(BSTR* out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    *out = to_bstr(n->text());
    return S_OK;
}

IFACEMETHODIMP NodeProvider::get_IsReadOnly(BOOL* out) {
    if (!out) return E_POINTER;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    *out = n->has_state(State::ReadOnly) || n->has_state(State::Disabled) ? TRUE : FALSE;
    return S_OK;
}

IFACEMETHODIMP NodeProvider::SetValue(double value) {
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    if (n->has_state(State::Disabled)) return UIA_E_ELEMENTNOTENABLED;
    if (n->has_state(State::ReadOnly)) return UIA_E_INVALIDOPERATION;
    if (const auto& v = n->value(); v && (value < v->minimum || value > v->maximum)) {
        return E_INVALIDARG;  // what the RangeValue contract asks for out-of-range values
    }
    ActionParams params;
    params.number_val = value;
    return n->perform_action(kActionSetValue, params) ? S_OK : UIA_E_INVALIDOPERATION;
}

namespace {

template <class F>
HRESULT range_field(Node* n, double* out, F field) {
    if (!out) return E_POINTER;
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    const auto& v = n->value();
    *out = v ? field(*v) : 0.0;
    return S_OK;
}

} // namespace

IFACEMETHODIMP NodeProvider::get_Value(double* out) {
    return range_field(node(), out, [](const ValueRange& v) { return v.current; });
}

IFACEMETHODIMP NodeProvider::get_Maximum(double* out) {
    return range_field(node(), out, [](const ValueRange& v) { return v.maximum; });
}

IFACEMETHODIMP NodeProvider::get_Minimum(double* out) {
    return range_field(node(), out, [](const ValueRange& v) { return v.minimum; });
}

IFACEMETHODIMP NodeProvider::get_LargeChange(double* out) {
    // The model carries one step; a page is ten of them.
    return range_field(node(), out, [](const ValueRange& v) { return v.step * 10.0; });
}

IFACEMETHODIMP NodeProvider::get_SmallChange(double* out) {
    return range_field(node(), out, [](const ValueRange& v) { return v.step; });
}

} // namespace broa11y::uia
