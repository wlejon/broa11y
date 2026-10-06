// LinuxBridge against the real AT-SPI stack.
//
// This process is the application. It starts a private session bus and the
// real at-spi-bus-launcher on it (which brings up the accessibility bus and,
// by D-Bus activation, at-spi2-registryd), exposes a tree through LinuxBridge,
// then runs atspi_client: a separate process built on libatspi, the library
// Orca and Accerciser use. The client finds the application through the
// registry's desktop, reads the tree, drives actions, values and text, and
// waits for the events; this process answers it from its own loop and checks
// what the client's requests did to the application.
#include "check.h"
#include "broa11y/linux_bridge.h"
#include "broa11y/tree.h"

#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern char** environ;

namespace {

constexpr const char* kName = "test_linux_atspi";

bool executable(const std::string& p) { return access(p.c_str(), X_OK) == 0; }

std::string find_in_path(const char* name) {
    const char* path = std::getenv("PATH");
    std::string all = path ? path : "/usr/bin:/bin";
    size_t start = 0;
    while (start <= all.size()) {
        size_t end = all.find(':', start);
        if (end == std::string::npos) end = all.size();
        std::string cand = all.substr(start, end - start) + "/" + name;
        if (executable(cand)) return cand;
        start = end + 1;
    }
    return {};
}

std::string find_launcher() {
    for (const char* p : {"/usr/libexec/at-spi-bus-launcher", "/usr/lib/at-spi-bus-launcher",
                          "/usr/lib/at-spi2-core/at-spi-bus-launcher", "/usr/libexec/at-spi2/at-spi-bus-launcher"}) {
        if (executable(p)) return p;
    }
    return {};
}

pid_t spawn(const std::vector<std::string>& argv) {
    std::vector<char*> args;
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    pid_t pid = -1;
    if (posix_spawn(&pid, args[0], nullptr, nullptr, args.data(), environ) != 0) return -1;
    return pid;
}

void stop(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    for (int i = 0; i < 100; ++i) {
        if (waitpid(pid, nullptr, WNOHANG) == pid) return;
        usleep(20000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

// A private session bus, so the test neither needs nor touches the user's.
struct PrivateBus {
    pid_t pid = -1;
    std::string address;

    bool start(const std::string& daemon) {
        std::string cmd = daemon + " --session --fork --print-address=1 --print-pid=1";
        FILE* f = popen(cmd.c_str(), "r");
        if (!f) return false;
        char line[512];
        if (fgets(line, sizeof line, f)) address = line;
        if (fgets(line, sizeof line, f)) pid = static_cast<pid_t>(std::atoi(line));
        pclose(f);
        while (!address.empty() && (address.back() == '\n' || address.back() == '\r')) address.pop_back();
        return !address.empty() && pid > 0;
    }

    ~PrivateBus() {
        if (pid > 0) kill(pid, SIGTERM);
    }
};

}  // namespace

int main() {
    const std::string daemon = find_in_path("dbus-daemon");
    if (daemon.empty()) bstest::skip(kName, "dbus-daemon is not installed (package dbus-daemon / dbus)");
    const std::string launcher = find_launcher();
    if (launcher.empty()) bstest::skip(kName, "at-spi-bus-launcher is not installed (package at-spi2-core)");
    const std::string client = BROA11Y_ATSPI_CLIENT;
    if (client.empty()) {
        bstest::skip(kName, "libatspi development files (pkg-config atspi-2) were not found at configure time, "
                            "so the libatspi client was not built");
    }

    using namespace broa11y;
    Tree tree;
    LinuxBridgeConfig cfg;
    cfg.app_name = "broa11y-atspi-test";
    cfg.window_origin = [] { return PointF{100, 100}; };

    // Before any bus exists the bridge must refuse, and say why.
    {
        unsetenv("AT_SPI_BUS_ADDRESS");
        setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/broa11y-test-bus", 1);
        LinuxBridge none(cfg);
        Tree empty;
        empty.create_node_with_role(Role::Window, 1);
        CHECK(!none.initialize(&empty));
        CHECK(!none.is_active());
        CHECK(!none.last_error().empty());
        std::printf("unavailable as expected: %s\n", none.last_error().c_str());
    }

    PrivateBus bus;
    REQUIRE(bus.start(daemon));
    setenv("DBUS_SESSION_BUS_ADDRESS", bus.address.c_str(), 1);
    // Keep the launcher and the client off any real display: no X root
    // property is written, and libatspi finds the bus through org.a11y.Bus.
    unsetenv("DISPLAY");
    unsetenv("WAYLAND_DISPLAY");
    unsetenv("AT_SPI_BUS_ADDRESS");

    pid_t launcher_pid = spawn({launcher, "--launch-immediately"});
    REQUIRE(launcher_pid > 0);

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
    button->add_action({.name = std::string(kActionActivate), .description = "Submits", .key_binding = "Return"});
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
            entry->set_selection(p.range_val);
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

    // The launcher needs a moment to claim org.a11y.Bus.
    LinuxBridge bridge(cfg);
    bool up = false;
    std::string why;
    for (int i = 0; i < 50 && !up; ++i) {
        up = bridge.initialize(&tree);
        if (!up) {
            why = bridge.last_error();
            usleep(100000);
        }
    }
    if (!up) {
        std::printf("initialize failed: %s\n", why.c_str());
        stop(launcher_pid);
        REQUIRE(false);
    }
    CHECK(bridge.is_active());
    CHECK(bridge.poll_fd() >= 0);
    CHECK(!bridge.bus_name().empty());

    pid_t client_pid = spawn({client, cfg.app_name});
    REQUIRE(client_pid > 0);

    int status = -1;
    bool exited = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline) {
        bridge.process_events();
        if (waitpid(client_pid, &status, WNOHANG) == client_pid) {
            exited = true;
            break;
        }
        pollfd pfd{bridge.poll_fd(), bridge.poll_events(), 0};
        int t = bridge.poll_timeout_ms();
        poll(&pfd, 1, t < 0 || t > 50 ? 50 : t);
    }
    if (!exited) {
        kill(client_pid, SIGKILL);
        waitpid(client_pid, &status, 0);
    }
    CHECK(exited);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // What the client's requests did, as the application saw them.
    CHECK_EQ(invoked, 1);
    CHECK_EQ(button->name(), "Submitted");
    CHECK(check->has_state(State::Checked));
    CHECK_EQ(slid, 75.0);
    CHECK_EQ(typed, "new \xE2\x9C\x93 text");
    CHECK_EQ(tree.focused_node_id(), 2u);
    CHECK_EQ(selected.start_offset, 7);  // characters 6..11 = bytes 7..13 ("wörld")
    CHECK_EQ(selected.end_offset, 13);

    bridge.shutdown();
    CHECK(!bridge.is_active());
    stop(launcher_pid);
    return bstest::finish(kName);
}
