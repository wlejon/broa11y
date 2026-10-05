#include "check.h"
#include "broa11y/win_bridge.h"
#include "broa11y/tree.h"
#include "../src/win/uia_constants.h"

int main() {
    broa11y::Tree tree;
    auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
    window->set_name("Desktop Window");
    window->set_bounds({0, 0, 1024, 768});

    auto* button = tree.create_node_with_role(broa11y::Role::Button, 2);
    button->set_name("Submit");
    button->set_description("Submits user profile");
    button->set_bounds({50, 50, 120, 40});
    button->set_state(broa11y::State::Focusable, true);
    tree.reparent_node(2, 1);

    auto* checkbox = tree.create_node_with_role(broa11y::Role::CheckBox, 3);
    checkbox->set_name("Agree to Terms");
    tree.reparent_node(3, 1);

    auto* slider = tree.create_node_with_role(broa11y::Role::Slider, 4);
    slider->set_value({.current = 25.0, .minimum = 0.0, .maximum = 100.0, .step = 5.0});
    tree.reparent_node(4, 1);

    broa11y::WinBridge bridge;
    bool ok = bridge.initialize(&tree);
    CHECK(ok);
    CHECK(bridge.is_active());
    CHECK_EQ(bridge.name(), "UI Automation (Windows)");

    // 1. Test Query Properties
    std::string btn_name = bridge.query_provider_property(2, broa11y::uia::kNamePropertyId);
    CHECK_EQ(btn_name, "Submit");

    std::string btn_help = bridge.query_provider_property(2, broa11y::uia::kHelpTextPropertyId);
    CHECK_EQ(btn_help, "Submits user profile");

    std::string btn_bounds = bridge.query_provider_property(2, broa11y::uia::kBoundingRectanglePropertyId);
    CHECK_EQ(btn_bounds, "[50, 50, 120, 40]");

    std::string slider_val = bridge.query_provider_property(4, broa11y::uia::kRangeValueValuePropertyId);
    CHECK(slider_val.find("25.00") != std::string::npos);

    // 2. Test Pattern Actions
    // Button Invoke
    bool btn_clicked = false;
    button->set_action_handler([&](broa11y::NodeId, std::string_view act, const broa11y::ActionParams&) {
        if (act == broa11y::kActionActivate) {
            btn_clicked = true;
            return true;
        }
        return false;
    });
    CHECK(bridge.execute_provider_action(2, broa11y::uia::kInvokePatternId, ""));
    CHECK(btn_clicked);

    // Checkbox Toggle
    CHECK(!checkbox->has_state(broa11y::State::Checked));
    CHECK(bridge.execute_provider_action(3, broa11y::uia::kTogglePatternId, ""));
    CHECK(checkbox->has_state(broa11y::State::Checked));

    // 3. Test Event Firing
    bridge.clear_emitted_events();
    CHECK_EQ(bridge.emitted_event_count(), 0u);

    // Change focus
    tree.set_focus(2);
    // Change property
    button->set_name("Submitted!");
    // Announcement
    tree.announce("Saved successfully");

    CHECK(bridge.emitted_event_count() >= 3u);
    auto ev_names = bridge.get_emitted_event_names();
    bool has_focus_ev = false;
    bool has_prop_ev = false;
    bool has_notif_ev = false;

    for (const auto& ev : ev_names) {
        if (ev.find("StateChange: focused") != std::string::npos) has_focus_ev = true;
        if (ev.find("PropertyChange: name") != std::string::npos) has_prop_ev = true;
        if (ev.find("Saved successfully") != std::string::npos) has_notif_ev = true;
    }

    CHECK(has_focus_ev);
    CHECK(has_prop_ev);
    CHECK(has_notif_ev);

    bridge.shutdown();
    CHECK(!bridge.is_active());

    return check::finish("test_win_bridge");
}
