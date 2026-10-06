// Text indexing shared by the model and the platform bridges.
//
// The model stores text as UTF-8 and its offsets (caret, selection,
// TerminalAccessibility queries) are UTF-8 byte offsets. The platforms count
// differently: AT-SPI in characters (code points), UI Automation and
// NSAccessibility in UTF-16 code units. TextIndex decodes a string once and
// converts between the three; text_unit_at() finds unit boundaries (character,
// word, line, paragraph, document) on code points, so every bridge and the
// terminal agree on where a word or a line starts.
#pragma once

#include "broa11y/types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace broa11y::text {

class TextIndex {
public:
    TextIndex() = default;
    explicit TextIndex(std::string_view utf8);

    // Number of characters (code points). Invalid UTF-8 bytes count as one
    // U+FFFD character each.
    [[nodiscard]] int32_t size() const noexcept { return static_cast<int32_t>(cps_.size()); }
    [[nodiscard]] int32_t byte_size() const noexcept { return bytes_.back(); }
    [[nodiscard]] int32_t utf16_size() const noexcept { return u16_.back(); }

    [[nodiscard]] char32_t at(int32_t cp) const noexcept { return cps_[static_cast<size_t>(cp)]; }

    // Conversions. Out-of-range inputs clamp; a byte or UTF-16 offset inside a
    // character rounds down to that character's start.
    [[nodiscard]] int32_t cp_from_byte(int32_t byte) const noexcept;
    [[nodiscard]] int32_t cp_from_utf16(int32_t unit) const noexcept;
    [[nodiscard]] int32_t byte_from_cp(int32_t cp) const noexcept;
    [[nodiscard]] int32_t utf16_from_cp(int32_t cp) const noexcept;

    // UTF-8 of characters [cp_start, cp_end).
    [[nodiscard]] std::string slice(int32_t cp_start, int32_t cp_end) const;
    // UTF-16 of characters [cp_start, cp_end).
    [[nodiscard]] std::u16string slice_utf16(int32_t cp_start, int32_t cp_end) const;

private:
    [[nodiscard]] int32_t clamp(int32_t cp) const noexcept;

    std::vector<char32_t> cps_;
    std::vector<int32_t> bytes_{0};  // size()+1 entries: byte offset of each character
    std::vector<int32_t> u16_{0};    // size()+1 entries: UTF-16 offset of each character
};

// The unit containing character `cp`, as [start, end) in characters. A
// position at or past the end belongs to the last unit. Word: a run of letters
// and digits, or a run of other non-newline characters; a newline is its own
// unit. Line: up to and including the '\n'. Paragraph: up to and including a
// blank line. Empty text yields {0, 0}.
[[nodiscard]] std::pair<int32_t, int32_t> unit_at(const TextIndex& text, int32_t cp,
                                                  TextGranularity unit);

[[nodiscard]] bool is_word_char(char32_t c) noexcept;

// UTF-8 encode / decode helpers.
void append_utf8(std::string& out, char32_t c);
[[nodiscard]] std::string utf16_to_utf8(std::u16string_view s);
[[nodiscard]] std::u16string utf8_to_utf16(std::string_view s);

} // namespace broa11y::text
