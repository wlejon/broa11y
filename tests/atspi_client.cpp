// The assistive-technology side of test_linux_atspi: libatspi, as Orca uses
// it, against the application named in argv[1]. Nothing in here knows
// broa11y; it sees only what the registry and the application's objects say.
// libatspi caches names and states and keeps the cache current from the
// events, so reading them back after a change also checks the events.
#include "check.h"

#include <atspi/atspi.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

using namespace std::chrono;

// Runs the GLib main context (where libatspi dispatches events) until pred
// holds or the timeout passes.
template <class Pred>
bool pump_until(Pred pred, milliseconds timeout = milliseconds(5000)) {
    auto end = steady_clock::now() + timeout;
    while (steady_clock::now() < end) {
        while (g_main_context_iteration(nullptr, FALSE)) {
        }
        if (pred()) return true;
        g_usleep(10000);
    }
    return pred();
}

std::string take(gchar* s) {
    std::string out = s ? s : "";
    g_free(s);
    return out;
}

std::string name_of(AtspiAccessible* a) { return take(atspi_accessible_get_name(a, nullptr)); }

bool has_state(AtspiAccessible* a, AtspiStateType st) {
    AtspiStateSet* set = atspi_accessible_get_state_set(a);
    bool on = set && atspi_state_set_contains(set, st);
    if (set) g_object_unref(set);
    return on;
}

AtspiAccessible* watched = nullptr;  // name changes are counted for this object only
std::atomic<int> name_events{0};
std::atomic<int> child_added_events{0};
std::atomic<int> announcements{0};
std::string last_name;
std::string last_announcement;

void on_event(AtspiEvent* ev, void*) {
    std::string type = ev->type ? ev->type : "";
    if (type.rfind("object:property-change:accessible-name", 0) == 0 && ev->source == watched) {
        if (G_VALUE_HOLDS_STRING(&ev->any_data)) last_name = g_value_get_string(&ev->any_data);
        ++name_events;
    } else if (type.rfind("object:children-changed:add", 0) == 0) {
        ++child_added_events;
    } else if (type.rfind("object:announcement", 0) == 0) {
        if (G_VALUE_HOLDS_STRING(&ev->any_data)) last_announcement = g_value_get_string(&ev->any_data);
        ++announcements;
    }
    g_boxed_free(ATSPI_TYPE_EVENT, ev);
}

}  // namespace

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    const std::string app_name = argv[1];
    REQUIRE(atspi_init() >= 0);

    // The registry lists embedded applications under the desktop.
    AtspiAccessible* desktop = atspi_get_desktop(0);
    REQUIRE(desktop != nullptr);
    AtspiAccessible* app = nullptr;
    pump_until([&] {
        gint n = atspi_accessible_get_child_count(desktop, nullptr);
        for (gint i = 0; i < n && !app; ++i) {
            AtspiAccessible* c = atspi_accessible_get_child_at_index(desktop, i, nullptr);
            if (c && name_of(c) == app_name) {
                app = c;
            } else if (c) {
                g_object_unref(c);
            }
        }
        return app != nullptr;
    });
    REQUIRE(app != nullptr);
    CHECK_EQ(atspi_accessible_get_role(app, nullptr), ATSPI_ROLE_APPLICATION);
    CHECK_EQ(take(atspi_accessible_get_toolkit_name(app, nullptr)), "broa11y");
    REQUIRE(atspi_accessible_get_child_count(app, nullptr) == 1);

    AtspiAccessible* frame = atspi_accessible_get_child_at_index(app, 0, nullptr);
    REQUIRE(frame != nullptr);
    CHECK_EQ(name_of(frame), "broa11y test window");
    CHECK_EQ(atspi_accessible_get_role(frame, nullptr), ATSPI_ROLE_FRAME);
    REQUIRE(atspi_accessible_get_child_count(frame, nullptr) == 5);

    const char* names[] = {"Submit", "Wrap lines", "Command", "Volume", "Status"};
    const AtspiRole roles[] = {ATSPI_ROLE_BUTTON, ATSPI_ROLE_CHECK_BOX, ATSPI_ROLE_ENTRY, ATSPI_ROLE_SLIDER,
                               ATSPI_ROLE_LABEL};
    AtspiAccessible* kid[5] = {};
    for (int i = 0; i < 5; ++i) {
        kid[i] = atspi_accessible_get_child_at_index(frame, i, nullptr);
        REQUIRE(kid[i] != nullptr);
        CHECK_EQ(name_of(kid[i]), names[i]);
        CHECK_EQ(atspi_accessible_get_role(kid[i], nullptr), roles[i]);
        CHECK_EQ(atspi_accessible_get_index_in_parent(kid[i], nullptr), i);
        AtspiAccessible* parent = atspi_accessible_get_parent(kid[i], nullptr);
        CHECK(parent == frame);  // libatspi hands out one object per path
        if (parent) g_object_unref(parent);
    }
    AtspiAccessible* button = kid[0];
    AtspiAccessible* check = kid[1];
    AtspiAccessible* entry = kid[2];
    AtspiAccessible* slider = kid[3];

    CHECK(has_state(button, ATSPI_STATE_FOCUSABLE));
    CHECK(has_state(button, ATSPI_STATE_ENABLED));
    CHECK(has_state(button, ATSPI_STATE_SENSITIVE));
    CHECK(has_state(button, ATSPI_STATE_SHOWING));
    CHECK(has_state(check, ATSPI_STATE_CHECKABLE));
    CHECK(!has_state(check, ATSPI_STATE_CHECKED));

    // ── Component: window-relative bounds, the window at (100, 100) ──
    {
        AtspiComponent* comp = atspi_accessible_get_component_iface(button);
        REQUIRE(comp != nullptr);
        AtspiRect* r = atspi_component_get_extents(comp, ATSPI_COORD_TYPE_SCREEN, nullptr);
        REQUIRE(r != nullptr);
        CHECK_EQ(r->x, 110);
        CHECK_EQ(r->y, 110);
        CHECK_EQ(r->width, 100);
        CHECK_EQ(r->height, 30);
        g_free(r);
        r = atspi_component_get_extents(comp, ATSPI_COORD_TYPE_WINDOW, nullptr);
        CHECK_EQ(r->x, 10);
        g_free(r);
        g_object_unref(comp);

        AtspiComponent* fc = atspi_accessible_get_component_iface(frame);
        AtspiAccessible* hit = atspi_component_get_accessible_at_point(fc, 20, 20, ATSPI_COORD_TYPE_WINDOW, nullptr);
        CHECK(hit == button);
        if (hit) g_object_unref(hit);
        g_object_unref(fc);
    }

    // ── Text: characters on the wire, the model's UTF-8 bytes behind ──
    {
        AtspiText* text = atspi_accessible_get_text_iface(entry);
        REQUIRE(text != nullptr);
        CHECK_EQ(atspi_text_get_character_count(text, nullptr), 13);
        CHECK_EQ(atspi_text_get_caret_offset(text, nullptr), 6);
        AtspiTextRange* word = atspi_text_get_string_at_offset(text, 6, ATSPI_TEXT_GRANULARITY_WORD, nullptr);
        REQUIRE(word != nullptr);
        CHECK_EQ(std::string(word->content), "w\xC3\xB6rld");
        CHECK_EQ(word->start_offset, 6);
        CHECK_EQ(word->end_offset, 11);
        g_boxed_free(ATSPI_TYPE_TEXT_RANGE, word);
        CHECK_EQ(take(atspi_text_get_text(text, 12, 13, nullptr)), "\xF0\x9F\x98\x80");
        CHECK_EQ(take(atspi_text_get_text(text, 0, -1, nullptr)), "h\xC3\xA9llo w\xC3\xB6rld \xF0\x9F\x98\x80");
        // Reaches the application as set_selection over bytes 7..13.
        CHECK(atspi_text_add_selection(text, 6, 11, nullptr));
        CHECK(pump_until([&] { return atspi_text_get_n_selections(text, nullptr) == 1; }));
        AtspiRange* sel = atspi_text_get_selection(text, 0, nullptr);
        REQUIRE(sel != nullptr);
        CHECK_EQ(sel->start_offset, 6);
        CHECK_EQ(sel->end_offset, 11);
        g_free(sel);
        g_object_unref(text);
    }

    // ── Action, and the events it causes ──
    watched = button;
    AtspiEventListener* listener = atspi_event_listener_new(on_event, nullptr, nullptr);
    for (const char* ev : {"object:property-change:accessible-name", "object:children-changed", "object:announcement"}) {
        GError* err = nullptr;
        CHECK(atspi_event_listener_register(listener, ev, &err));
        if (err) {
            std::fprintf(stderr, "register %s: %s\n", ev, err->message);
            g_error_free(err);
        }
    }
    {
        AtspiAction* action = atspi_accessible_get_action_iface(button);
        REQUIRE(action != nullptr);
        CHECK_EQ(atspi_action_get_n_actions(action, nullptr), 1);
        CHECK_EQ(take(atspi_action_get_action_name(action, 0, nullptr)), "activate");
        CHECK_EQ(take(atspi_action_get_key_binding(action, 0, nullptr)), "Return");
        CHECK(atspi_action_do_action(action, 0, nullptr));
        g_object_unref(action);
    }
    CHECK(pump_until([] { return name_events.load() > 0; }));
    CHECK_EQ(last_name, "Submitted");
    CHECK_EQ(name_of(button), "Submitted");
    CHECK(pump_until([] { return child_added_events.load() > 0; }));
    CHECK(pump_until([] { return announcements.load() > 0; }));
    CHECK_EQ(last_announcement, "Form submitted");
    CHECK(pump_until([&] { return atspi_accessible_get_child_count(frame, nullptr) == 6; }));
    if (AtspiAccessible* result = atspi_accessible_get_child_at_index(frame, 5, nullptr)) {
        CHECK_EQ(name_of(result), "Result");
        g_object_unref(result);
    }

    // Check box: its activate toggles it, and the cached state follows.
    {
        AtspiAction* action = atspi_accessible_get_action_iface(check);
        REQUIRE(action != nullptr);
        CHECK(atspi_action_do_action(action, 0, nullptr));
        g_object_unref(action);
        CHECK(pump_until([&] { return has_state(check, ATSPI_STATE_CHECKED); }));
    }

    // ── Value ──
    {
        AtspiValue* value = atspi_accessible_get_value_iface(slider);
        REQUIRE(value != nullptr);
        CHECK_EQ(atspi_value_get_current_value(value, nullptr), 50.0);
        CHECK_EQ(atspi_value_get_minimum_value(value, nullptr), 0.0);
        CHECK_EQ(atspi_value_get_maximum_value(value, nullptr), 100.0);
        CHECK_EQ(atspi_value_get_minimum_increment(value, nullptr), 5.0);
        CHECK(atspi_value_set_current_value(value, 75.0, nullptr));
        CHECK_EQ(atspi_value_get_current_value(value, nullptr), 75.0);
        g_object_unref(value);
    }

    // ── EditableText ──
    {
        AtspiEditableText* edit = atspi_accessible_get_editable_text_iface(entry);
        REQUIRE(edit != nullptr);
        CHECK(atspi_editable_text_set_text_contents(edit, "new \xE2\x9C\x93 text", nullptr));
        g_object_unref(edit);
        AtspiText* text = atspi_accessible_get_text_iface(entry);
        CHECK_EQ(take(atspi_text_get_text(text, 0, -1, nullptr)), "new \xE2\x9C\x93 text");
        CHECK_EQ(atspi_text_get_character_count(text, nullptr), 10);
        g_object_unref(text);
    }

    // ── Focus ──
    {
        AtspiComponent* comp = atspi_accessible_get_component_iface(button);
        CHECK(atspi_component_grab_focus(comp, nullptr));
        g_object_unref(comp);
        CHECK(pump_until([&] { return has_state(button, ATSPI_STATE_FOCUSED); }));
    }

    g_object_unref(listener);
    for (AtspiAccessible* k : kid) g_object_unref(k);
    g_object_unref(frame);
    g_object_unref(app);
    g_object_unref(desktop);
    int rc = bstest::finish("test_linux_atspi (libatspi client)");
    atspi_exit();
    return rc;
}
