// UI Automation providers for broa11y nodes.
//
// One NodeProvider COM object per node, cached in the bridge's Context. The
// tree root's provider is also the fragment root that the host window returns
// from WM_GETOBJECT. Providers declare ProviderOptions_UseComThreading and are
// created on the window's STA thread, so UIA delivers every call on that
// thread through its message loop and the tree is never touched elsewhere.
//
// After the bridge shuts down, or once a node is removed, its provider is
// disconnected and every call answers UIA_E_ELEMENTNOTAVAILABLE.
#pragma once

#include "broa11y/tree.h"

#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

namespace broa11y::uia {

class NodeProvider;

// Shared by the bridge and every provider; providers keep it alive after the
// bridge is gone, so a late client call finds tree == null, not freed memory.
// drop_all() breaks the provider <-> context reference cycle at shutdown.
struct Context : std::enable_shared_from_this<Context> {
    Tree* tree = nullptr;  // null once the bridge has shut down
    HWND hwnd = nullptr;
    std::wstring app_name;
    std::unordered_map<NodeId, NodeProvider*> providers;  // each holds one reference

    // The cached provider for a node, created on first use; null when the node
    // is not in the tree. The returned pointer is borrowed.
    NodeProvider* provider(NodeId id);
    // Disconnects and drops a node's provider (node removed).
    void drop(NodeId id);
    // Disconnects and drops every provider (bridge shutdown).
    void drop_all();
};

// Client pixels of ctx.hwnd to screen pixels, and back.
RECT to_screen(const Context& ctx, const RectF& r);
PointF to_client(const Context& ctx, double x, double y);

BSTR to_bstr(std::string_view utf8);
std::string from_wide(const wchar_t* s);

class NodeProvider final : public IRawElementProviderSimple,
                           public IRawElementProviderFragment,
                           public IRawElementProviderFragmentRoot,
                           public IInvokeProvider,
                           public IToggleProvider,
                           public IValueProvider,
                           public IRangeValueProvider,
                           public ITextProvider {
public:
    NodeProvider(std::shared_ptr<Context> ctx, NodeId id);

    [[nodiscard]] NodeId id() const noexcept { return id_; }
    // The node, or null when it (or the bridge) is gone.
    [[nodiscard]] Node* node() const;
    [[nodiscard]] const std::shared_ptr<Context>& context() const noexcept { return ctx_; }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IRawElementProviderSimple
    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* out) override;
    IFACEMETHODIMP GetPatternProvider(PATTERNID pattern, IUnknown** out) override;
    IFACEMETHODIMP GetPropertyValue(PROPERTYID property, VARIANT* out) override;
    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** out) override;

    // IRawElementProviderFragment
    IFACEMETHODIMP Navigate(NavigateDirection direction, IRawElementProviderFragment** out) override;
    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** out) override;
    IFACEMETHODIMP get_BoundingRectangle(UiaRect* out) override;
    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** out) override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** out) override;

    // IRawElementProviderFragmentRoot
    IFACEMETHODIMP ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** out) override;
    IFACEMETHODIMP GetFocus(IRawElementProviderFragment** out) override;

    // IInvokeProvider
    IFACEMETHODIMP Invoke() override;

    // IToggleProvider
    IFACEMETHODIMP Toggle() override;
    IFACEMETHODIMP get_ToggleState(ToggleState* out) override;

    // IValueProvider (get_IsReadOnly is shared with IRangeValueProvider)
    IFACEMETHODIMP SetValue(LPCWSTR value) override;
    IFACEMETHODIMP get_Value(BSTR* out) override;
    IFACEMETHODIMP get_IsReadOnly(BOOL* out) override;

    // IRangeValueProvider
    IFACEMETHODIMP SetValue(double value) override;
    IFACEMETHODIMP get_Value(double* out) override;
    IFACEMETHODIMP get_Maximum(double* out) override;
    IFACEMETHODIMP get_Minimum(double* out) override;
    IFACEMETHODIMP get_LargeChange(double* out) override;
    IFACEMETHODIMP get_SmallChange(double* out) override;

    // ITextProvider (uia_text.cpp)
    IFACEMETHODIMP GetSelection(SAFEARRAY** out) override;
    IFACEMETHODIMP GetVisibleRanges(SAFEARRAY** out) override;
    IFACEMETHODIMP RangeFromChild(IRawElementProviderSimple* child, ITextRangeProvider** out) override;
    IFACEMETHODIMP RangeFromPoint(UiaPoint point, ITextRangeProvider** out) override;
    IFACEMETHODIMP get_DocumentRange(ITextRangeProvider** out) override;
    IFACEMETHODIMP get_SupportedTextSelection(SupportedTextSelection* out) override;

    // Which patterns this node supports right now.
    [[nodiscard]] bool supports(PATTERNID pattern) const;

private:
    ~NodeProvider() = default;

    [[nodiscard]] bool is_root() const;

    std::atomic<ULONG> refs_{1};
    std::shared_ptr<Context> ctx_;
    NodeId id_;
};

// A text range over one node's text, in UTF-16 code units (uia_text.cpp).
ITextRangeProvider* make_text_range(NodeProvider* owner, int32_t start, int32_t end);

} // namespace broa11y::uia
