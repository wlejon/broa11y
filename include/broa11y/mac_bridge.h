#pragma once

#include "broa11y/bridge.h"
#include "broa11y/types.h"

#include <memory>
#include <string>
#include <string_view>

namespace broa11y {

struct MacBridgeConfig {
    std::string app_name = "bro";
    // The NSView (passed as void*, unretained) whose area the tree describes.
    // Required: the tree's root becomes the view's accessibility child, and
    // node bounds are read as top-left-origin points within the view.
    void* ns_view = nullptr;
};

// NSAccessibility provider: one NSAccessibilityElement per node, parented
// under the view. AppKit asks for accessibility on the main thread, so
// initialize() must run there and the tree is only touched from it.
// process_events() does nothing; the main run loop delivers the requests.
//
// The host view has to hand the tree to AppKit: override accessibilityChildren
// to return element_for(tree->root_id()), and accessibilityHitTest: and
// accessibilityFocusedUIElement to use element_at_screen_point() and
// element_for(tree->focused_node_id()). initialize() also calls the view's
// setAccessibilityChildren:, but AppKit's own traversal from the window does
// not consult that on a plain NSView (observed on macOS 26), so the overrides
// are what makes the tree reachable.
class MacBridge : public Bridge {
public:
    explicit MacBridge(MacBridgeConfig config = {});
    ~MacBridge() override;

    MacBridge(const MacBridge&) = delete;
    MacBridge& operator=(const MacBridge&) = delete;

    bool initialize(Tree* tree) override;
    void shutdown() override;
    void handle_event(const Event& event) override;
    void process_events() override;
    [[nodiscard]] std::string_view name() const noexcept override { return "NSAccessibility (macOS)"; }
    [[nodiscard]] bool is_active() const noexcept override;
    [[nodiscard]] std::string last_error() const override;

    // The element for a node (an NSAccessibilityElement*, unretained), for an
    // application whose view answers accessibilityHitTest: or
    // accessibilityFocusedUIElement itself; null when the node is absent.
    [[nodiscard]] void* element_for(NodeId id) const;
    // The node at a point in screen coordinates (AppKit's bottom-left origin).
    [[nodiscard]] void* element_at_screen_point(double x, double y) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace broa11y
