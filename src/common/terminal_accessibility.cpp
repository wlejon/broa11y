#include "broa11y/terminal.h"
#include "broa11y/node.h"
#include "broa11y/tree.h"

#include <algorithm>

namespace broa11y {

TerminalAccessibility::TerminalAccessibility() = default;
TerminalAccessibility::~TerminalAccessibility() = default;

TerminalAccessibility::TerminalAccessibility(TerminalAccessibility&& other) noexcept
    : tree_(other.tree_),
      node_id_(other.node_id_),
      rows_(other.rows_),
      cols_(other.cols_),
      cursor_row_(other.cursor_row_),
      cursor_col_(other.cursor_col_),
      sel_start_row_(other.sel_start_row_),
      sel_start_col_(other.sel_start_col_),
      sel_end_row_(other.sel_end_row_),
      sel_end_col_(other.sel_end_col_),
      lines_(std::move(other.lines_)),
      cache_dirty_(other.cache_dirty_),
      cached_full_text_(std::move(other.cached_full_text_)),
      line_start_offsets_(std::move(other.line_start_offsets_)) {
    other.tree_ = nullptr;
    other.node_id_ = kInvalidNodeId;
}

TerminalAccessibility& TerminalAccessibility::operator=(TerminalAccessibility&& other) noexcept {
    if (this != &other) {
        tree_ = other.tree_;
        node_id_ = other.node_id_;
        rows_ = other.rows_;
        cols_ = other.cols_;
        cursor_row_ = other.cursor_row_;
        cursor_col_ = other.cursor_col_;
        sel_start_row_ = other.sel_start_row_;
        sel_start_col_ = other.sel_start_col_;
        sel_end_row_ = other.sel_end_row_;
        sel_end_col_ = other.sel_end_col_;
        lines_ = std::move(other.lines_);
        cache_dirty_ = other.cache_dirty_;
        cached_full_text_ = std::move(other.cached_full_text_);
        line_start_offsets_ = std::move(other.line_start_offsets_);

        other.tree_ = nullptr;
        other.node_id_ = kInvalidNodeId;
    }
    return *this;
}

void TerminalAccessibility::attach_to_node(Tree* tree, NodeId node_id) {
    tree_ = tree;
    node_id_ = node_id;
    if (tree_ && node_id_ != kInvalidNodeId) {
        Node* node = tree_->get_node(node_id_);
        if (node) {
            node->set_role(Role::Terminal);
            sync_to_node();
        }
    }
}

void TerminalAccessibility::detach() {
    tree_ = nullptr;
    node_id_ = kInvalidNodeId;
}

void TerminalAccessibility::set_grid_size(int32_t rows, int32_t cols) {
    rows_ = rows > 0 ? rows : 1;
    cols_ = cols > 0 ? cols : 1;
}

int32_t TerminalAccessibility::row_count() const noexcept {
    return static_cast<int32_t>(lines_.size());
}

void TerminalAccessibility::set_line(int32_t row, std::string_view text, bool wrapped) {
    if (row < 0) return;
    size_t urow = static_cast<size_t>(row);
    if (urow >= lines_.size()) {
        lines_.resize(urow + 1);
    }
    lines_[urow] = TerminalLine{
        .text = std::string(text),
        .wrapped = wrapped
    };
    cache_dirty_ = true;
    sync_to_node();
}

void TerminalAccessibility::append_line(std::string_view text, bool wrapped) {
    lines_.push_back(TerminalLine{
        .text = std::string(text),
        .wrapped = wrapped
    });
    cache_dirty_ = true;
    sync_to_node();
}

void TerminalAccessibility::clear() {
    lines_.clear();
    cursor_row_ = 0;
    cursor_col_ = 0;
    clear_selection();
    cache_dirty_ = true;
    sync_to_node();
}

void TerminalAccessibility::set_cursor(int32_t row, int32_t col) {
    cursor_row_ = row >= 0 ? row : 0;
    cursor_col_ = col >= 0 ? col : 0;
    if (tree_ && node_id_ != kInvalidNodeId) {
        Node* node = tree_->get_node(node_id_);
        if (node) {
            node->set_caret_offset(cursor_offset());
        }
    }
}

int32_t TerminalAccessibility::cursor_offset() const noexcept {
    return pos_to_offset(cursor_row_, cursor_col_);
}

void TerminalAccessibility::set_selection(int32_t start_row, int32_t start_col, int32_t end_row, int32_t end_col) {
    sel_start_row_ = start_row;
    sel_start_col_ = start_col;
    sel_end_row_ = end_row;
    sel_end_col_ = end_col;
    if (tree_ && node_id_ != kInvalidNodeId) {
        Node* node = tree_->get_node(node_id_);
        if (node) {
            node->set_selection(selection_range());
        }
    }
}

void TerminalAccessibility::clear_selection() {
    sel_start_row_ = -1;
    sel_start_col_ = -1;
    sel_end_row_ = -1;
    sel_end_col_ = -1;
    if (tree_ && node_id_ != kInvalidNodeId) {
        Node* node = tree_->get_node(node_id_);
        if (node) {
            node->set_selection(TextRange{0, 0});
        }
    }
}

bool TerminalAccessibility::has_selection() const noexcept {
    return sel_start_row_ >= 0 && sel_end_row_ >= 0;
}

TextRange TerminalAccessibility::selection_range() const noexcept {
    if (!has_selection()) {
        return TextRange{0, 0};
    }
    int32_t s_off = pos_to_offset(sel_start_row_, sel_start_col_);
    int32_t e_off = pos_to_offset(sel_end_row_, sel_end_col_);
    if (s_off > e_off) std::swap(s_off, e_off);
    return TextRange{s_off, e_off};
}

std::string TerminalAccessibility::selected_text() const {
    if (!has_selection()) return {};
    rebuild_cache();
    TextRange range = selection_range();
    if (range.start_offset >= 0 && range.end_offset <= static_cast<int32_t>(cached_full_text_.size())) {
        return cached_full_text_.substr(static_cast<size_t>(range.start_offset),
                                        static_cast<size_t>(range.length()));
    }
    return {};
}

void TerminalAccessibility::ring_bell() {
    if (tree_) {
        tree_->announce("Alert: Terminal bell", AnnouncementPriority::Assertive, node_id_);
    }
}

void TerminalAccessibility::announce(std::string_view message, AnnouncementPriority priority) {
    if (tree_) {
        tree_->announce(message, priority, node_id_);
    }
}

int32_t TerminalAccessibility::character_count() const noexcept {
    rebuild_cache();
    return static_cast<int32_t>(cached_full_text_.size());
}

std::string TerminalAccessibility::get_full_text() const {
    rebuild_cache();
    return cached_full_text_;
}

std::string TerminalAccessibility::get_line_text(int32_t row) const {
    if (row >= 0 && static_cast<size_t>(row) < lines_.size()) {
        return lines_[static_cast<size_t>(row)].text;
    }
    return {};
}

std::pair<int32_t, int32_t> TerminalAccessibility::offset_to_pos(int32_t offset) const noexcept {
    rebuild_cache();
    if (offset <= 0 || line_start_offsets_.empty()) {
        return {0, 0};
    }

    auto it = std::upper_bound(line_start_offsets_.begin(), line_start_offsets_.end(), offset);
    size_t line_idx = static_cast<size_t>(std::distance(line_start_offsets_.begin(), it) - 1);
    int32_t col = offset - line_start_offsets_[line_idx];
    return {static_cast<int32_t>(line_idx), col};
}

int32_t TerminalAccessibility::pos_to_offset(int32_t row, int32_t col) const noexcept {
    rebuild_cache();
    if (row < 0 || line_start_offsets_.empty()) return 0;
    size_t urow = static_cast<size_t>(row);
    if (urow >= line_start_offsets_.size()) {
        return static_cast<int32_t>(cached_full_text_.size());
    }

    int32_t base = line_start_offsets_[urow];
    int32_t line_len = static_cast<int32_t>(lines_[urow].text.size());
    int32_t offset = base + (col < line_len ? col : line_len);
    return offset;
}

void TerminalAccessibility::rebuild_cache() const {
    if (!cache_dirty_) return;

    cached_full_text_.clear();
    line_start_offsets_.clear();

    for (size_t i = 0; i < lines_.size(); ++i) {
        line_start_offsets_.push_back(static_cast<int32_t>(cached_full_text_.size()));
        cached_full_text_.append(lines_[i].text);
        if (!lines_[i].wrapped) {
            cached_full_text_.push_back('\n');
        }
    }

    cache_dirty_ = false;
}

void TerminalAccessibility::sync_to_node() {
    if (!tree_ || node_id_ == kInvalidNodeId) return;
    Node* node = tree_->get_node(node_id_);
    if (!node) return;

    rebuild_cache();
    node->set_text(cached_full_text_);
    node->set_caret_offset(cursor_offset());
    if (has_selection()) {
        node->set_selection(selection_range());
    }
}

} // namespace broa11y
