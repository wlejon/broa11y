#include "check.h"
#include "broa11y/tree.h"

int main() {
    broa11y::Tree tree;
    auto* button = tree.create_node_with_role(broa11y::Role::Button, 1);
    button->set_name("Click Me");

    button->add_action({
        .name = std::string(broa11y::kActionActivate),
        .description = "Presses the button",
        .key_binding = "Return"
    });

    button->add_action({
        .name = "custom_greet",
        .description = "Says hello",
        .key_binding = "Ctrl+H"
    });

    CHECK_EQ(button->actions().size(), 2u);
    CHECK_EQ(button->actions()[0].name, broa11y::kActionActivate);
    CHECK_EQ(button->actions()[1].name, "custom_greet");

    // Test default perform_action without handler (returns true if action exists)
    CHECK(button->perform_action(broa11y::kActionActivate));
    CHECK(button->perform_action("custom_greet"));
    CHECK(!button->perform_action("non_existent_action"));

    // Test with ActionHandler
    bool activate_called = false;
    std::string greeted_who;
    double slider_val = 0.0;

    button->set_action_handler([&](broa11y::NodeId id, std::string_view act, const broa11y::ActionParams& params) {
        CHECK_EQ(id, 1u);
        if (act == broa11y::kActionActivate) {
            activate_called = true;
            return true;
        } else if (act == "custom_greet") {
            greeted_who = params.string_val;
            return true;
        } else if (act == broa11y::kActionSetValue) {
            slider_val = params.number_val;
            return true;
        }
        return false;
    });

    CHECK(button->perform_action(broa11y::kActionActivate));
    CHECK(activate_called);

    broa11y::ActionParams greet_params;
    greet_params.string_val = "Alice";
    CHECK(button->perform_action("custom_greet", greet_params));
    CHECK_EQ(greeted_who, "Alice");

    broa11y::ActionParams val_params;
    val_params.number_val = 42.5;
    CHECK(button->perform_action(broa11y::kActionSetValue, val_params));
    CHECK_EQ(slider_val, 42.5);

    CHECK(!button->perform_action("unknown"));

    return check::finish("test_actions");
}
