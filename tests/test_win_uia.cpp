// WinBridge against the real UI Automation client.
//
// This process is the application: a hidden window on an STA thread carries
// the tree through WinBridge and pumps messages. A second process (this same
// binary with --client <hwnd>, test_win_uia_client.cpp) is the assistive
// technology: it uses IUIAutomation, the API Narrator and NVDA's UIA support
// use, to read the tree, drive the patterns and wait for the events. What the
// client did then shows up here, in the action handlers and the tree.
#include "check.h"
#include "broa11y/tree.h"
#include "broa11y/win_bridge.h"

#include <windows.h>

#include <cstdio>
#include <string>

int run_client(HWND hwnd);  // test_win_uia_client.cpp

namespace {

LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

HWND make_hidden_window() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"broa11y_test_window";
    RegisterClassW(&wc);
    // A popup has no non-client parts, so the window's UIA children are
    // exactly the tree's. Never shown.
    return CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"host title", WS_POPUP, 100, 100, 400, 300,
                           nullptr, nullptr, wc.hInstance, nullptr);
}

}  // namespace

int main(int argc, char** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (argc == 3 && std::string(argv[1]) == "--client") {
        return run_client(reinterpret_cast<HWND>(static_cast<uintptr_t>(std::stoull(argv[2]))));
    }

    using namespace broa11y;
    Tree tree;
    Node* root = tree.create_node_with_role(Role::Window, 1);
    root->set_name("broa11y test window");
    root->set_bounds({0, 0, 400, 300});

    auto add = [&](NodeId id, Role role, const char* name, RectF bounds) {
        Node* n = tree.create_node_with_role(role, id);
        n->set_name(name);
        n->set_bounds(bounds);
        n->set_state(State::Focusable);
        tree.reparent_node(id, 1);
        return n;
    };
    Node* button = add(2, Role::Button, "Submit", {10, 10, 100, 30});
    Node* check = add(3, Role::CheckBox, "Wrap lines", {10, 50, 100, 20});
    Node* entry = add(4, Role::TextInput, "Command", {10, 80, 300, 24});
    Node* slider = add(5, Role::Slider, "Volume", {10, 120, 200, 20});
    add(6, Role::Label, "Status", {10, 150, 200, 20});

    button->add_action({.name = std::string(kActionActivate), .description = "Submits", .key_binding = "Enter"});
    check->add_action({.name = std::string(kActionActivate), .description = "Toggles", .key_binding = ""});
    entry->set_text("h\xC3\xA9llo w\xC3\xB6rld \xF0\x9F\x98\x80");  // héllo wörld 😀
    entry->set_caret_offset(7);                                    // before the 'w'
    slider->set_value({.current = 50, .minimum = 0, .maximum = 100, .step = 5});

    int invoked = 0;
    std::string typed;
    double slid = -1;
    TextRange selected{-1, -1};
    auto handler = [&](NodeId id, std::string_view action, const ActionParams& p) {
        if (id == 2 && action == kActionActivate) {
            ++invoked;
            button->set_name("Submitted");
            Node* result = tree.create_node_with_role(Role::Label, 7);
            result->set_name("Result");
            result->set_bounds({10, 180, 200, 20});
            tree.reparent_node(7, 1);
            tree.announce("Form submitted", AnnouncementPriority::Assertive, 2);
            return true;
        }
        if (id == 3 && action == kActionActivate) {
            check->set_state(State::Checked, !check->has_state(State::Checked));
            return true;
        }
        if (id == 4 && action == kActionSetValue) {
            typed = p.string_val;
            entry->set_text(p.string_val);
            return true;
        }
        if (id == 4 && action == kActionSetSelection) {
            selected = p.range_val;
            return true;
        }
        if (id == 5 && action == kActionSetValue) {
            slid = p.number_val;
            ValueRange v = *slider->value();
            v.current = p.number_val;
            slider->set_value(v);
            return true;
        }
        if (action == kActionFocus) return tree.set_focus(id);
        return false;
    };
    for (NodeId id = 2; id <= 6; ++id) tree.get_node(id)->set_action_handler(handler);

    // Failures the bridge must report rather than pretend.
    {
        WinBridge no_window;
        CHECK(!no_window.initialize(&tree));
        CHECK(!no_window.last_error().empty());
        CHECK(!no_window.is_active());
    }

    HWND hwnd = make_hidden_window();
    REQUIRE(hwnd != nullptr);
    WinBridge bridge({.app_name = "broa11y-test", .hwnd = hwnd});
    if (!bridge.initialize(&tree)) {
        std::printf("initialize failed: %s\n", bridge.last_error().c_str());
        REQUIRE(false);
    }
    CHECK(bridge.is_active());
    CHECK(bridge.last_error().empty());

    // The client process.
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --client " +
                       std::to_wstring(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    REQUIRE(CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi));

    // Serve it: every UIA call reaches the providers through this loop.
    const ULONGLONG deadline = GetTickCount64() + 45000;
    bool exited = false;
    while (GetTickCount64() < deadline) {
        DWORD r = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 100, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) {
            exited = true;
            break;
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (!exited) TerminateProcess(pi.hProcess, 99);
    DWORD client_rc = 99;
    GetExitCodeProcess(pi.hProcess, &client_rc);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CHECK(exited);
    CHECK_EQ(client_rc, 0ul);

    // What the client did, as the application saw it.
    CHECK_EQ(invoked, 1);
    CHECK_EQ(button->name(), "Submitted");
    CHECK(check->has_state(State::Checked));
    CHECK_EQ(typed, "new \xE2\x9C\x93 text");
    CHECK_EQ(slid, 75.0);
    CHECK_EQ(tree.focused_node_id(), 2u);
    // The client selected "wörld" through the text pattern: bytes 7..13.
    CHECK_EQ(selected.start_offset, 7);
    CHECK_EQ(selected.end_offset, 13);

    bridge.shutdown();
    CHECK(!bridge.is_active());
    DestroyWindow(hwnd);
    std::printf("Note: point hit testing (ElementFromPoint) is not exercised: the window is never shown, so "
                "the point belongs to whatever is on screen there\n");
    return bstest::finish("test_win_uia");
}
