#include "check.h"
#include "broa11y/linux_bridge.h"
#include "broa11y/tree.h"

int main() {
    broa11y::Tree tree;
    auto* window = tree.create_node_with_role(broa11y::Role::Window, 1);
    window->set_name("Main App Window");
    window->set_bounds({0, 0, 800, 600});

    auto* button = tree.create_node_with_role(broa11y::Role::Button, 2);
    button->set_name("Submit");
    button->set_bounds({10, 10, 100, 30});
    button->add_action({.name = "activate", .description = "Clicks the button", .key_binding = ""});
    tree.reparent_node(2, 1);

    auto* text_input = tree.create_node_with_role(broa11y::Role::TextInput, 3);
    text_input->set_text("Hello AT-SPI");
    text_input->set_caret_offset(5);
    tree.reparent_node(3, 1);

    auto* slider = tree.create_node_with_role(broa11y::Role::Slider, 4);
    slider->set_value({.current = 50.0, .minimum = 0.0, .maximum = 100.0, .step = 5.0});
    tree.reparent_node(4, 1);

    broa11y::LinuxBridgeConfig cfg;
    cfg.headless_mock = true;
    broa11y::LinuxBridge bridge(cfg);

    bool ok = bridge.initialize(&tree);
    CHECK(ok);
    CHECK(bridge.is_active());
    CHECK_EQ(bridge.name(), "AT-SPI 2 (Linux)");

    // 1. Test Accessible interface method calls
    std::string root_role_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/root", "org.a11y.atspi.Accessible", "GetRole"
    );
    // Window role in AT-SPI is 75
    CHECK(root_role_rep.find("75") != std::string::npos);

    std::string btn_name_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/2", "org.a11y.atspi.Accessible", "Name"
    );
    CHECK(btn_name_rep.find("Submit") != std::string::npos);

    std::string children_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/root", "org.a11y.atspi.Accessible", "GetChildren"
    );
    CHECK(children_rep.find("/org/a11y/atspi/accessible/2") != std::string::npos);
    CHECK(children_rep.find("/org/a11y/atspi/accessible/3") != std::string::npos);

    // 2. Test Component interface method calls
    std::string contains_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/2", "org.a11y.atspi.Component", "Contains", {"20", "20", "0"}
    );
    CHECK(contains_rep.find("true") != std::string::npos);

    std::string extents_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/2", "org.a11y.atspi.Component", "GetExtents", {"0"}
    );
    CHECK(extents_rep.find("(10, 10, 100, 30)") != std::string::npos);

    // 3. Test Action interface
    std::string nactions_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/2", "org.a11y.atspi.Action", "GetNActions"
    );
    CHECK(nactions_rep.find("1") != std::string::npos);

    std::string do_action_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/2", "org.a11y.atspi.Action", "DoAction", {"0"}
    );
    CHECK(do_action_rep.find("true") != std::string::npos);

    // 4. Test Text interface
    std::string char_count_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/3", "org.a11y.atspi.Text", "GetCharacterCount"
    );
    CHECK(char_count_rep.find("12") != std::string::npos);

    std::string text_offset_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/3", "org.a11y.atspi.Text", "GetTextAtOffset", {"0", "1"} // Word at 0
    );
    CHECK(text_offset_rep.find("Hello") != std::string::npos);

    // 5. Test EditableText interface
    bridge.handle_method_call(
        "/org/a11y/atspi/accessible/3", "org.a11y.atspi.EditableText", "InsertText", {"5", ", World"}
    );
    CHECK_EQ(text_input->text(), "Hello, World AT-SPI");

    // 6. Test Value interface
    std::string val_rep = bridge.handle_method_call(
        "/org/a11y/atspi/accessible/4", "org.a11y.atspi.Value", "CurrentValue"
    );
    CHECK(val_rep.find("50.0") != std::string::npos);

    bridge.handle_method_call(
        "/org/a11y/atspi/accessible/4", "org.a11y.atspi.Value", "SetCurrentValue", {"75.0"}
    );
    CHECK_EQ(slider->value()->current, 75.0);

    // 7. Test Signal Emissions on tree mutations
    bridge.clear_emitted_signals();
    CHECK_EQ(bridge.emitted_signal_count(), 0u);

    // Mutate state
    button->set_state(broa11y::State::Focused, true);
    // Mutate property
    button->set_name("Submitted!");
    // Caret moved
    text_input->set_caret_offset(8);
    // Announcement
    tree.announce("Action completed", broa11y::AnnouncementPriority::Polite);

    bridge.process_events();

    auto signal_names = bridge.get_emitted_signal_names();
    CHECK(signal_names.size() >= 4u);

    bool has_state_sig = false;
    bool has_prop_sig = false;
    bool has_caret_sig = false;
    bool has_announcement_sig = false;

    for (const auto& sig : signal_names) {
        if (sig.find("StateChanged") != std::string::npos) has_state_sig = true;
        if (sig.find("PropertyChange:AccessibleName") != std::string::npos) has_prop_sig = true;
        if (sig.find("TextCaretMoved") != std::string::npos) has_caret_sig = true;
        if (sig.find("Announcement") != std::string::npos) has_announcement_sig = true;
    }

    CHECK(has_state_sig);
    CHECK(has_prop_sig);
    CHECK(has_caret_sig);
    CHECK(has_announcement_sig);

    bridge.shutdown();
    CHECK(!bridge.is_active());

    return check::finish("test_linux_bridge");
}
