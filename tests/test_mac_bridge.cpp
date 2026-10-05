#include "check.h"
#include "broa11y/mac_bridge.h"
#include "broa11y/tree.h"
#include "../src/mac/mac_constants.h"

int main() {
    broa11y::Tree tree;
    auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
    window->set_name("Mac Window");
    window->set_bounds({100, 100, 640, 480});

    auto* button = tree.create_node_with_role(broa11y::Role::Button, 2);
    button->set_name("Click Me");
    button->set_bounds({120, 120, 80, 30});
    tree.reparent_node(2, 1);

    auto* text = tree.create_node_with_role(broa11y::Role::TextInput, 3);
    text->set_text("macOS Accessibility");
    tree.reparent_node(3, 1);

    auto* slider = tree.create_node_with_role(broa11y::Role::Slider, 4);
    slider->set_value({.current = 10.0, .minimum = 0.0, .maximum = 100.0, .step = 5.0});
    tree.reparent_node(4, 1);

    broa11y::MacBridge bridge;
    bool ok = bridge.initialize(&tree);
    CHECK(ok);
    CHECK(bridge.is_active());
    CHECK_EQ(bridge.name(), "NSAccessibility (macOS)");

    // 1. Test Query Attributes
    std::string win_role = bridge.query_element_attribute(1, broa11y::mac::kRoleAttribute);
    CHECK_EQ(win_role, "AXWindow");

    std::string btn_role = bridge.query_element_attribute(2, broa11y::mac::kRoleAttribute);
    CHECK_EQ(btn_role, "AXButton");

    std::string btn_title = bridge.query_element_attribute(2, broa11y::mac::kTitleAttribute);
    CHECK_EQ(btn_title, "Click Me");

    std::string txt_val = bridge.query_element_attribute(3, broa11y::mac::kValueAttribute);
    CHECK_EQ(txt_val, "macOS Accessibility");

    std::string slider_val = bridge.query_element_attribute(4, broa11y::mac::kValueAttribute);
    CHECK(slider_val.find("10.00") != std::string::npos);

    // 2. Parameterized Attributes
    std::string sub = bridge.query_parameterized_attribute(3, broa11y::mac::kStringForRangeAttribute, "0,5");
    CHECK_EQ(sub, "macOS");

    // 3. Actions
    bool btn_pressed = false;
    button->set_action_handler([&](broa11y::NodeId, std::string_view act, const broa11y::ActionParams&) {
        if (act == broa11y::kActionActivate) {
            btn_pressed = true;
            return true;
        }
        return false;
    });

    CHECK(bridge.perform_element_action(2, broa11y::mac::kPressAction));
    CHECK(btn_pressed);

    CHECK(bridge.perform_element_action(4, broa11y::mac::kIncrementAction));
    CHECK_EQ(slider->value()->current, 15.0);

    // 4. Notifications
    bridge.clear_emitted_notifications();
    CHECK_EQ(bridge.emitted_notification_count(), 0u);

    // Set focus
    tree.set_focus(2);
    // Change title
    button->set_name("Pressed!");
    // Announcement
    tree.announce("VoiceOver notice");

    CHECK(bridge.emitted_notification_count() >= 3u);
    auto notifs = bridge.get_emitted_notifications();
    bool has_focus_notif = false;
    bool has_title_notif = false;
    bool has_announcement_notif = false;

    for (const auto& n : notifs) {
        if (n == broa11y::mac::kFocusedUIElementChangedNotification) has_focus_notif = true;
        if (n == broa11y::mac::kTitleChangedNotification) has_title_notif = true;
        if (n == broa11y::mac::kAnnouncementRequestedNotification) has_announcement_notif = true;
    }

    CHECK(has_focus_notif);
    CHECK(has_title_notif);
    CHECK(has_announcement_notif);

    bridge.shutdown();
    CHECK(!bridge.is_active());

    return check::finish("test_mac_bridge");
}
