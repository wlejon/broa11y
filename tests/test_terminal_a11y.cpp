#include "check.h"
#include "broa11y/terminal.h"
#include "broa11y/tree.h"

int main() {
    broa11y::Tree tree;
    auto* term_node = tree.create_node(10);
    CHECK(term_node != nullptr);

    broa11y::TerminalAccessibility term;
    term.attach_to_node(&tree, 10);
    CHECK(term.is_attached());
    CHECK_EQ(term.attached_node_id(), 10u);
    CHECK(term_node->role() == broa11y::Role::Terminal);

    // Track events
    int caret_events = 0;
    int selection_events = 0;
    int announcements = 0;
    std::string last_announcement;

    tree.add_listener([&](const broa11y::Event& ev) {
        if (ev.type == broa11y::EventType::CaretMoved) {
            ++caret_events;
        } else if (ev.type == broa11y::EventType::TextSelectionChanged) {
            ++selection_events;
        } else if (ev.type == broa11y::EventType::Announcement) {
            ++announcements;
            if (const auto* p = ev.get_if<broa11y::AnnouncementPayload>()) {
                last_announcement = p->message;
            }
        }
    });

    // 1. Append lines
    // Line 0: "user@host:~$ ls -la" (hard break \n)
    // Line 1: "total 42" (hard break \n)
    // Line 2: "drwxr-xr-x 2 user user " (wrapped)
    // Line 3: "4096 Oct 5 12:00 ." (hard break \n)
    term.append_line("user@host:~$ ls -la", false);
    term.append_line("total 42", false);
    term.append_line("drwxr-xr-x 2 user user ", true);
    term.append_line("4096 Oct 5 12:00 .", false);

    CHECK_EQ(term.row_count(), 4);
    std::string full_text = term.get_full_text();
    // Line 0 has \n, Line 1 has \n, Line 2 does NOT have \n because wrapped=true, Line 3 has \n.
    // "user@host:~$ ls -la\ntotal 42\ndrwxr-xr-x 2 user user 4096 Oct 5 12:00 .\n"
    CHECK(full_text.find("user@host:~$ ls -la\n") == 0);
    CHECK_EQ(term_node->text(), full_text);

    // 2. Cursor tracking and caret offset
    term.set_cursor(0, 5); // on 'h' in user@host
    CHECK_EQ(term.cursor_offset(), 5);
    CHECK_EQ(term_node->caret_offset(), 5);
    CHECK(caret_events > 0);

    // 3. Selection tracking
    term.set_selection(0, 0, 0, 4); // "user"
    CHECK(term.has_selection());
    CHECK_EQ(term.selected_text(), "user");
    CHECK(selection_events > 0);

    term.clear_selection();
    CHECK(!term.has_selection());

    // 4. Coordinate conversions
    // Line 0 starts at 0, length = 19 + 1 (\n) = 20.
    // Line 1 starts at 20 ("total 42\n"), length = 8 + 1 = 9.
    // Line 2 starts at 29 ("drwxr-xr-x 2 user user "), length = 23 (no newline because wrapped)
    // Line 3 starts at 52 ("4096 Oct 5 12:00 .\n")
    auto pos0 = term.offset_to_pos(5);
    CHECK_EQ(pos0.first, 0);
    CHECK_EQ(pos0.second, 5);
    CHECK_EQ(term.pos_to_offset(0, 5), 5);

    auto pos1 = term.offset_to_pos(23); // 20 + 3 => line 1, col 3 ('a' in total)
    CHECK_EQ(pos1.first, 1);
    CHECK_EQ(pos1.second, 3);
    CHECK_EQ(term.pos_to_offset(1, 3), 23);

    // 5. Navigation at granularity
    // Test Character
    int32_t s = 0, e = 0;
    std::string ch = term.get_text_at_offset(5, broa11y::TextGranularity::Character, &s, &e);
    CHECK_EQ(ch, "h");
    CHECK_EQ(s, 5);
    CHECK_EQ(e, 6);

    // Test Word on "user"
    std::string word = term.get_text_at_offset(2, broa11y::TextGranularity::Word, &s, &e);
    CHECK_EQ(word, "user");
    CHECK_EQ(s, 0);
    CHECK_EQ(e, 4);

    // Test Word on "host" (offset 5 is 'h')
    std::string host_word = term.get_text_at_offset(5, broa11y::TextGranularity::Word, &s, &e);
    CHECK_EQ(host_word, "host");

    // Test Line
    std::string line0 = term.get_text_at_offset(5, broa11y::TextGranularity::Line, &s, &e);
    CHECK_EQ(line0, "user@host:~$ ls -la\n");
    CHECK_EQ(s, 0);
    CHECK_EQ(e, 20);

    // Test text after and text before
    std::string next_word = term.get_text_after_offset(2, broa11y::TextGranularity::Word, &s, &e);
    CHECK(!next_word.empty());

    // 6. Announcements & Bell
    term.ring_bell();
    CHECK_EQ(announcements, 1);
    CHECK_EQ(last_announcement, "Alert: Terminal bell");

    term.announce("Process exited with code 0", broa11y::AnnouncementPriority::Polite);
    CHECK_EQ(announcements, 2);
    CHECK_EQ(last_announcement, "Process exited with code 0");

    return check::finish("test_terminal_a11y");
}
