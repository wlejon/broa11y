#include "broa11y/terminal.h"

#include <cctype>
#include <string_view>

namespace broa11y {

namespace {

bool is_word_char(char c) {
    auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
}

} // namespace

void find_char_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end) {
    int32_t len = static_cast<int32_t>(text.size());
    if (len == 0 || offset < 0 || offset >= len) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return;
    }
    if (out_start) *out_start = offset;
    if (out_end) *out_end = offset + 1;
}

void find_word_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end) {
    int32_t len = static_cast<int32_t>(text.size());
    if (len == 0) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return;
    }

    if (offset < 0) offset = 0;
    if (offset >= len) offset = len - 1;

    int32_t start = offset;
    int32_t end = offset;

    bool on_word = is_word_char(text[static_cast<size_t>(offset)]);

    if (on_word) {
        while (start > 0 && is_word_char(text[static_cast<size_t>(start - 1)])) {
            --start;
        }
        while (end < len && is_word_char(text[static_cast<size_t>(end)])) {
            ++end;
        }
    } else {
        while (start > 0 && !is_word_char(text[static_cast<size_t>(start - 1)]) && text[static_cast<size_t>(start - 1)] != '\n') {
            --start;
        }
        while (end < len && !is_word_char(text[static_cast<size_t>(end)]) && text[static_cast<size_t>(end)] != '\n') {
            ++end;
        }
    }

    if (start == end && end < len) {
        ++end;
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

void find_line_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end) {
    int32_t len = static_cast<int32_t>(text.size());
    if (len == 0) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return;
    }

    if (offset < 0) offset = 0;
    if (offset >= len) offset = len - 1;

    int32_t start = offset;
    while (start > 0 && text[static_cast<size_t>(start - 1)] != '\n') {
        --start;
    }

    int32_t end = offset;
    while (end < len && text[static_cast<size_t>(end)] != '\n') {
        ++end;
    }
    if (end < len && text[static_cast<size_t>(end)] == '\n') {
        ++end; // include newline in line boundary
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

void find_paragraph_bounds(std::string_view text, int32_t offset, int32_t* out_start, int32_t* out_end) {
    // For terminal, paragraphs can be delineated by blank lines (\n\n) or line boundaries
    int32_t len = static_cast<int32_t>(text.size());
    if (len == 0) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return;
    }

    if (offset < 0) offset = 0;
    if (offset >= len) offset = len - 1;

    int32_t start = offset;
    while (start > 1) {
        if (text[static_cast<size_t>(start - 1)] == '\n' && text[static_cast<size_t>(start - 2)] == '\n') {
            break;
        }
        --start;
    }
    if (start == 1 && text[0] == '\n') start = 0;

    int32_t end = offset;
    while (end < len - 1) {
        if (text[static_cast<size_t>(end)] == '\n' && text[static_cast<size_t>(end + 1)] == '\n') {
            end += 2;
            break;
        }
        ++end;
    }
    if (end == len - 1) end = len;

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

std::string TerminalAccessibility::get_text_at_offset(int32_t offset,
                                                      TextGranularity granularity,
                                                      int32_t* out_start,
                                                      int32_t* out_end) const {
    rebuild_cache();
    int32_t len = static_cast<int32_t>(cached_full_text_.size());
    if (len == 0) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return {};
    }

    int32_t start = 0;
    int32_t end = 0;

    switch (granularity) {
        case TextGranularity::Character:
            find_char_bounds(cached_full_text_, offset, &start, &end);
            break;
        case TextGranularity::Word:
            find_word_bounds(cached_full_text_, offset, &start, &end);
            break;
        case TextGranularity::Line:
            find_line_bounds(cached_full_text_, offset, &start, &end);
            break;
        case TextGranularity::Paragraph:
            find_paragraph_bounds(cached_full_text_, offset, &start, &end);
            break;
        case TextGranularity::Document:
            start = 0;
            end = len;
            break;
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;

    if (start >= 0 && end > start && start < len) {
        return cached_full_text_.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    }
    return {};
}

std::string TerminalAccessibility::get_text_before_offset(int32_t offset,
                                                         TextGranularity granularity,
                                                         int32_t* out_start,
                                                         int32_t* out_end) const {
    int32_t cur_start = 0;
    int32_t cur_end = 0;
    static_cast<void>(get_text_at_offset(offset, granularity, &cur_start, &cur_end));

    int32_t prev_offset = cur_start - 1;
    if (prev_offset < 0) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return {};
    }

    return get_text_at_offset(prev_offset, granularity, out_start, out_end);
}

std::string TerminalAccessibility::get_text_after_offset(int32_t offset,
                                                        TextGranularity granularity,
                                                        int32_t* out_start,
                                                        int32_t* out_end) const {
    int32_t cur_start = 0;
    int32_t cur_end = 0;
    static_cast<void>(get_text_at_offset(offset, granularity, &cur_start, &cur_end));

    int32_t next_offset = cur_end;
    rebuild_cache();
    if (next_offset >= static_cast<int32_t>(cached_full_text_.size())) {
        if (out_start) *out_start = static_cast<int32_t>(cached_full_text_.size());
        if (out_end) *out_end = static_cast<int32_t>(cached_full_text_.size());
        return {};
    }

    return get_text_at_offset(next_offset, granularity, out_start, out_end);
}

} // namespace broa11y
