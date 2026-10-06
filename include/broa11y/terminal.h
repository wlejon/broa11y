#pragma once

#include "broa11y/events.h"
#include "broa11y/types.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace broa11y {

class Tree;
class Node;

struct TerminalLine {
    std::string text;
    bool wrapped = false; // true if this line is a soft wrap continuation of previous line
};

// Exposes a terminal grid as one text node. Rows hold UTF-8; a column is a
// character (code point) index within its row. Every offset this class takes
// or returns, like the node's caret and selection, is a UTF-8 byte offset into
// get_full_text(); character_count() counts characters. The bridges convert to
// what each platform counts (characters on AT-SPI, UTF-16 units on UIA and
// NSAccessibility).
class TerminalAccessibility {
public:
    TerminalAccessibility();
    ~TerminalAccessibility();

    TerminalAccessibility(const TerminalAccessibility&) = delete;
    TerminalAccessibility& operator=(const TerminalAccessibility&) = delete;
    TerminalAccessibility(TerminalAccessibility&&) noexcept;
    TerminalAccessibility& operator=(TerminalAccessibility&&) noexcept;

    void attach_to_node(Tree* tree, NodeId node_id);
    void detach();
    [[nodiscard]] bool is_attached() const noexcept { return tree_ != nullptr && node_id_ != kInvalidNodeId; }
    [[nodiscard]] NodeId attached_node_id() const noexcept { return node_id_; }

    void set_grid_size(int32_t rows, int32_t cols);
    [[nodiscard]] int32_t row_count() const noexcept;
    [[nodiscard]] int32_t col_count() const noexcept { return cols_; }

    void set_line(int32_t row, std::string_view text, bool wrapped = false);
    void append_line(std::string_view text, bool wrapped = false);
    void clear();

    void set_cursor(int32_t row, int32_t col);
    [[nodiscard]] std::pair<int32_t, int32_t> cursor() const noexcept { return {cursor_row_, cursor_col_}; }
    [[nodiscard]] int32_t cursor_offset() const noexcept;

    void set_selection(int32_t start_row, int32_t start_col, int32_t end_row, int32_t end_col);
    void clear_selection();
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] TextRange selection_range() const noexcept;
    [[nodiscard]] std::string selected_text() const;

    void ring_bell();
    void announce(std::string_view message, AnnouncementPriority priority = AnnouncementPriority::Polite);

    // Text query interface
    [[nodiscard]] int32_t character_count() const noexcept;
    [[nodiscard]] std::string get_full_text() const;
    [[nodiscard]] std::string get_line_text(int32_t row) const;

    [[nodiscard]] std::string get_text_at_offset(int32_t offset,
                                                TextGranularity granularity,
                                                int32_t* out_start = nullptr,
                                                int32_t* out_end = nullptr) const;

    [[nodiscard]] std::string get_text_before_offset(int32_t offset,
                                                   TextGranularity granularity,
                                                   int32_t* out_start = nullptr,
                                                   int32_t* out_end = nullptr) const;

    [[nodiscard]] std::string get_text_after_offset(int32_t offset,
                                                  TextGranularity granularity,
                                                  int32_t* out_start = nullptr,
                                                  int32_t* out_end = nullptr) const;

    // Coordinate conversions
    [[nodiscard]] std::pair<int32_t, int32_t> offset_to_pos(int32_t offset) const noexcept;
    [[nodiscard]] int32_t pos_to_offset(int32_t row, int32_t col) const noexcept;

    void sync_to_node();

private:
    void rebuild_cache() const;

    Tree* tree_ = nullptr;
    NodeId node_id_ = kInvalidNodeId;

    int32_t rows_ = 24;
    int32_t cols_ = 80;
    int32_t cursor_row_ = 0;
    int32_t cursor_col_ = 0;

    int32_t sel_start_row_ = -1;
    int32_t sel_start_col_ = -1;
    int32_t sel_end_row_ = -1;
    int32_t sel_end_col_ = -1;

    std::vector<TerminalLine> lines_;

    mutable bool cache_dirty_ = true;
    mutable std::string cached_full_text_;
    mutable std::vector<int32_t> line_start_offsets_;
};

} // namespace broa11y
