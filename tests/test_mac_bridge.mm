// MacBridge through AppKit.
//
// A real borderless NSWindow (never ordered on screen) holds a view that
// forwards its accessibility children, hit testing and focus to the bridge,
// as an application's view must. AppKit's own window traversal (the
// attribute API on the NSWindow) has to find the tree's root through that
// view; past that the checks use the NSAccessibility protocol and AppKit's
// helpers (NSAccessibilityRoleDescription, NSAccessibilityUnignoredChildren),
// the calls AppKit serves VoiceOver from.
//
// Not covered here: a cross-process AXUIElement client needs the
// Accessibility (TCC) permission, which CI machines do not grant, so the
// notifications the bridge posts are not observed.
#include "check.h"
#include "broa11y/mac_bridge.h"
#include "broa11y/tree.h"

#import <AppKit/AppKit.h>

#include <string>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface ForwardingView : NSView
@property(nonatomic) broa11y::MacBridge* bridge;
@property(nonatomic) broa11y::Tree* tree;
@end

@implementation ForwardingView
- (NSArray*)accessibilityChildren {
    id root = (__bridge id)self.bridge->element_for(self.tree->root_id());
    return root ? @[ root ] : @[];
}
- (id)accessibilityHitTest:(NSPoint)point {
    id hit = (__bridge id)self.bridge->element_at_screen_point(point.x, point.y);
    return hit ? hit : self;
}
- (id)accessibilityFocusedUIElement {
    id f = (__bridge id)self.bridge->element_for(self.tree->focused_node_id());
    return f ? f : self;
}
@end

namespace {

std::string str(NSString* v) { return v ? std::string([v UTF8String]) : std::string("(nil)"); }

}  // namespace

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        using broa11y::NodeId;
        using broa11y::Role;
        using broa11y::State;

        broa11y::Tree tree;
        broa11y::Node* root = tree.create_node_with_role(Role::Window, 1);
        root->set_name("broa11y test window");
        root->set_bounds({0, 0, 400, 300});
        auto add = [&](NodeId id, Role role, const char* name, broa11y::RectF bounds) {
            broa11y::Node* n = tree.create_node_with_role(role, id);
            n->set_name(name);
            n->set_bounds(bounds);
            n->set_state(State::Focusable);
            tree.reparent_node(id, 1);
            return n;
        };
        broa11y::Node* button = add(2, Role::Button, "Submit", {10, 10, 100, 30});
        broa11y::Node* check = add(3, Role::CheckBox, "Wrap lines", {10, 50, 100, 20});
        broa11y::Node* entry = add(4, Role::TextInput, "Command", {10, 80, 300, 24});
        broa11y::Node* slider = add(5, Role::Slider, "Volume", {10, 120, 200, 20});
        add(6, Role::Label, "Status", {10, 150, 200, 20});
        button->add_action({.name = std::string(broa11y::kActionActivate), .description = "Submits", .key_binding = ""});
        check->add_action({.name = std::string(broa11y::kActionActivate), .description = "Toggles", .key_binding = ""});
        entry->set_text("h\xC3\xA9llo w\xC3\xB6rld \xF0\x9F\x98\x80");  // héllo wörld 😀
        entry->set_caret_offset(7);
        slider->set_value({.current = 50, .minimum = 0, .maximum = 100, .step = 5});

        int invoked = 0;
        double slid = -1;
        std::string typed;
        broa11y::TextRange selected{-1, -1};  // qualified: Carbon has a TextRange too
        auto handler = [&](NodeId id, std::string_view action, const broa11y::ActionParams& p) {
            if (id == 2 && action == broa11y::kActionActivate) {
                ++invoked;
                button->set_name("Submitted");
                return true;
            }
            if (id == 3 && action == broa11y::kActionActivate) {
                check->set_state(State::Checked, !check->has_state(State::Checked));
                return true;
            }
            if (id == 4 && action == broa11y::kActionSetValue) {
                typed = p.string_val;
                entry->set_text(p.string_val);
                return true;
            }
            if (id == 4 && action == broa11y::kActionSetSelection) {
                selected = p.range_val;
                return true;
            }
            if (id == 5 && action == broa11y::kActionSetValue) {
                slid = p.number_val;
                broa11y::ValueRange v = *slider->value();
                v.current = p.number_val;
                slider->set_value(v);
                return true;
            }
            if (action == broa11y::kActionFocus) return tree.set_focus(id);
            return false;
        };
        for (NodeId id = 2; id <= 6; ++id) tree.get_node(id)->set_action_handler(handler);

        {
            broa11y::MacBridge no_view;
            CHECK(!no_view.initialize(&tree));
            CHECK(!no_view.last_error().empty());
        }

        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 400, 300)
                                                       styleMask:NSWindowStyleMaskBorderless
                                                         backing:NSBackingStoreBuffered
                                                           defer:YES];
        ForwardingView* view = [[ForwardingView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300)];
        window.contentView = view;

        broa11y::MacBridge bridge({.app_name = "broa11y-test", .ns_view = (__bridge void*)view});
        view.bridge = &bridge;
        view.tree = &tree;
        if (!bridge.initialize(&tree)) {
            std::printf("initialize failed: %s\n", bridge.last_error().c_str());
            REQUIRE(false);
        }
        CHECK(bridge.is_active());

        // AppKit's traversal from the window sees through the (ignored) view
        // to the tree's root.
        NSArray* window_kids = [window accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
        REQUIRE(window_kids.count == 1);
        NSAccessibilityElement* win = window_kids[0];
        CHECK(win == (__bridge id)bridge.element_for(1));
        CHECK_EQ(str(win.accessibilityRole), "AXWindow");
        CHECK_EQ(str(win.accessibilityTitle), "broa11y test window");
        CHECK(win.accessibilityParent == view);
        CHECK(NSAccessibilityUnignoredAncestor(win) == win);

        NSArray* kids = NSAccessibilityUnignoredChildren(win.accessibilityChildren);
        REQUIRE(kids.count == 5);
        const char* roles[] = {"AXButton", "AXCheckBox", "AXTextField", "AXSlider", "AXStaticText"};
        const char* names[] = {"Submit", "Wrap lines", "Command", "Volume", "Status"};
        for (NSUInteger i = 0; i < 5; ++i) {
            NSAccessibilityElement* k = kids[i];
            CHECK_EQ(str(k.accessibilityRole), roles[i]);
            CHECK_EQ(str(k.accessibilityLabel), names[i]);
            CHECK(k.accessibilityParent == win);
        }
        NSAccessibilityElement* ax_button = kids[0];
        NSAccessibilityElement* ax_check = kids[1];
        NSAccessibilityElement* ax_entry = kids[2];
        NSAccessibilityElement* ax_slider = kids[3];
        CHECK_EQ(str(ax_button.accessibilityRoleDescription), str(NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil)));

        // Frame: top-left (10, 10) in a 300-point-high unflipped view whose
        // window sits at (100, 100) is (110, 360) in screen coordinates.
        NSRect f = ax_button.accessibilityFrame;
        CHECK_EQ(f.origin.x, 110.0);
        CHECK_EQ(f.origin.y, 360.0);
        CHECK_EQ(f.size.width, 100.0);
        CHECK_EQ(f.size.height, 30.0);
        CHECK([view accessibilityHitTest:NSMakePoint(120, 370)] == ax_button);
        CHECK([win accessibilityHitTest:NSMakePoint(120, 370)] == ax_button);

        // Press.
        CHECK([ax_button accessibilityPerformPress]);
        CHECK_EQ(invoked, 1);
        CHECK_EQ(str(ax_button.accessibilityLabel), "Submitted");

        // Check box value follows its state.
        CHECK_EQ([ax_check.accessibilityValue intValue], 0);
        CHECK([ax_check accessibilityPerformPress]);
        CHECK_EQ([ax_check.accessibilityValue intValue], 1);

        // Slider: value, bounds, setting it, and increment.
        CHECK_EQ([ax_slider.accessibilityValue doubleValue], 50.0);
        CHECK_EQ([ax_slider.accessibilityMaxValue doubleValue], 100.0);
        ax_slider.accessibilityValue = @75;
        CHECK_EQ(slid, 75.0);
        CHECK([ax_slider accessibilityPerformIncrement]);
        CHECK_EQ(slid, 80.0);
        CHECK_EQ([ax_slider.accessibilityValue doubleValue], 80.0);

        // Text: UTF-16 ranges over the model's UTF-8.
        CHECK_EQ(ax_entry.accessibilityNumberOfCharacters, 14);
        NSRange sel = ax_entry.accessibilitySelectedTextRange;
        CHECK_EQ(sel.location, 6u);
        CHECK_EQ(sel.length, 0u);
        CHECK_EQ(str([ax_entry accessibilityStringForRange:NSMakeRange(6, 5)]), "w\xC3\xB6rld");
        NSRange emoji = [ax_entry accessibilityRangeForIndex:12];
        CHECK_EQ(emoji.location, 12u);
        CHECK_EQ(emoji.length, 2u);
        ax_entry.accessibilitySelectedTextRange = NSMakeRange(6, 5);
        CHECK_EQ(selected.start_offset, 7);
        CHECK_EQ(selected.end_offset, 13);
        ax_entry.accessibilityValue = @"new ✓ text";
        CHECK_EQ(typed, "new \xE2\x9C\x93 text");
        CHECK_EQ(str(ax_entry.accessibilityValue), "new \xE2\x9C\x93 text");

        // Focus.
        ax_button.accessibilityFocused = YES;
        CHECK_EQ(tree.focused_node_id(), 2u);
        CHECK(ax_button.isAccessibilityFocused);
        CHECK([view accessibilityFocusedUIElement] == ax_button);

        // A removed node's element goes inert.
        NSAccessibilityElement* status = kids[4];
        tree.remove_node(6);
        CHECK_EQ(win.accessibilityChildren.count, 4u);
        CHECK(!status.isAccessibilityElement);

        bridge.shutdown();
        CHECK(!bridge.is_active());
        CHECK(!win.isAccessibilityElement);
        std::printf("Note: posted NSAccessibility notifications are not observed in-process; a cross-process "
                    "AXUIElement observer needs the Accessibility permission\n");
        return bstest::finish("test_mac_bridge");
    }
}
