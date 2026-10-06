#include "common/text_util.h"

#include <algorithm>

namespace broa11y::text {

namespace {

// Decodes one character at s[i]; returns its length in bytes (at least 1).
// Malformed, overlong, surrogate and out-of-range sequences decode to U+FFFD
// one byte at a time.
size_t decode_one(std::string_view s, size_t i, char32_t& out) {
    auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) {
        out = b0;
        return 1;
    }
    size_t len = 0;
    char32_t cp = 0;
    char32_t min = 0;
    if ((b0 & 0xE0) == 0xC0) {
        len = 2; cp = b0 & 0x1F; min = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        len = 3; cp = b0 & 0x0F; min = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        len = 4; cp = b0 & 0x07; min = 0x10000;
    } else {
        out = 0xFFFD;
        return 1;
    }
    if (i + len > s.size()) {
        out = 0xFFFD;
        return 1;
    }
    for (size_t k = 1; k < len; ++k) {
        auto b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0) != 0x80) {
            out = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        out = 0xFFFD;
        return 1;
    }
    out = cp;
    return len;
}

bool is_newline(char32_t c) { return c == U'\n'; }

} // namespace

TextIndex::TextIndex(std::string_view utf8) {
    cps_.reserve(utf8.size());
    bytes_.reserve(utf8.size() + 1);
    u16_.reserve(utf8.size() + 1);
    size_t i = 0;
    int32_t u16 = 0;
    while (i < utf8.size()) {
        char32_t c = 0;
        i += decode_one(utf8, i, c);
        cps_.push_back(c);
        u16 += c >= 0x10000 ? 2 : 1;
        bytes_.push_back(static_cast<int32_t>(i));
        u16_.push_back(u16);
    }
}

int32_t TextIndex::clamp(int32_t cp) const noexcept {
    return std::clamp(cp, 0, size());
}

int32_t TextIndex::cp_from_byte(int32_t byte) const noexcept {
    if (byte <= 0) return 0;
    auto it = std::upper_bound(bytes_.begin(), bytes_.end(), byte);
    return static_cast<int32_t>(it - bytes_.begin()) - 1;
}

int32_t TextIndex::cp_from_utf16(int32_t unit) const noexcept {
    if (unit <= 0) return 0;
    auto it = std::upper_bound(u16_.begin(), u16_.end(), unit);
    return static_cast<int32_t>(it - u16_.begin()) - 1;
}

int32_t TextIndex::byte_from_cp(int32_t cp) const noexcept {
    return bytes_[static_cast<size_t>(clamp(cp))];
}

int32_t TextIndex::utf16_from_cp(int32_t cp) const noexcept {
    return u16_[static_cast<size_t>(clamp(cp))];
}

std::string TextIndex::slice(int32_t cp_start, int32_t cp_end) const {
    std::string out;
    // Re-encode rather than copy bytes, so invalid input comes out as U+FFFD
    // and every platform string is valid UTF-8.
    for (int32_t k = clamp(cp_start); k < clamp(cp_end); ++k) {
        append_utf8(out, cps_[static_cast<size_t>(k)]);
    }
    return out;
}

std::u16string TextIndex::slice_utf16(int32_t cp_start, int32_t cp_end) const {
    std::u16string out;
    for (int32_t k = clamp(cp_start); k < clamp(cp_end); ++k) {
        char32_t c = cps_[static_cast<size_t>(k)];
        if (c >= 0x10000) {
            c -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (c >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (c & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(c));
        }
    }
    return out;
}

bool is_word_char(char32_t c) noexcept {
    if (c < 0x80) {
        return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') ||
               (c >= U'A' && c <= U'Z') || c == U'_';
    }
    // Outside ASCII: everything but spaces and general punctuation counts as
    // part of a word. That keeps accented Latin, Greek, Cyrillic and CJK runs
    // together without a Unicode property table.
    if (c == 0x00A0 || c == 0x1680 || (c >= 0x2000 && c <= 0x206F) || c == 0x3000 ||
        (c >= 0x3001 && c <= 0x3003) || c == 0xFEFF || c == 0xFFFD) {
        return false;
    }
    return true;
}

std::pair<int32_t, int32_t> unit_at(const TextIndex& t, int32_t cp, TextGranularity unit) {
    const int32_t n = t.size();
    if (n == 0) return {0, 0};
    cp = std::clamp(cp, 0, n - 1);

    switch (unit) {
        case TextGranularity::Character:
            return {cp, cp + 1};

        case TextGranularity::Word: {
            char32_t c = t.at(cp);
            if (is_newline(c)) return {cp, cp + 1};
            const bool word = is_word_char(c);
            auto same = [&](int32_t k) {
                char32_t d = t.at(k);
                return !is_newline(d) && is_word_char(d) == word;
            };
            int32_t s = cp;
            while (s > 0 && same(s - 1)) --s;
            int32_t e = cp + 1;
            while (e < n && same(e)) ++e;
            return {s, e};
        }

        case TextGranularity::Line: {
            int32_t s = cp;
            // A position on a '\n' belongs to the line that newline ends.
            while (s > 0 && !is_newline(t.at(s - 1))) --s;
            int32_t e = cp;
            while (e < n && !is_newline(t.at(e))) ++e;
            if (e < n) ++e;
            return {s, e};
        }

        case TextGranularity::Paragraph: {
            // A paragraph ends after a blank line ("\n\n") or at the end.
            auto ends_paragraph = [&](int32_t k) {  // k is the index just past a '\n'
                return k >= 2 && is_newline(t.at(k - 1)) && is_newline(t.at(k - 2));
            };
            int32_t s = cp;
            while (s > 0 && !ends_paragraph(s)) --s;
            int32_t e = cp + 1;
            while (e < n && !ends_paragraph(e)) ++e;
            return {s, e};
        }

        case TextGranularity::Document:
            return {0, n};
    }
    return {cp, cp + 1};
}

void append_utf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
}

std::string utf16_to_utf8(std::u16string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char32_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (static_cast<char32_t>(s[i + 1]) - 0xDC00);
            ++i;
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            c = 0xFFFD;
        }
        append_utf8(out, c);
    }
    return out;
}

std::u16string utf8_to_utf16(std::string_view s) {
    TextIndex t(s);
    return t.slice_utf16(0, t.size());
}

} // namespace broa11y::text
