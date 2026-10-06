// text::TextIndex and unit_at: the conversions every bridge relies on to turn
// the model's UTF-8 byte offsets into AT-SPI characters and UIA / AppKit
// UTF-16 units.
#include "check.h"
#include "common/text_util.h"

using broa11y::TextGranularity;
using broa11y::text::TextIndex;
using broa11y::text::unit_at;

int main() {
    // "aé😀b": a (1 byte, 1 unit), é (2 bytes, 1 unit), 😀 (4 bytes, 2 units), b
    const std::string s = "a\xC3\xA9\xF0\x9F\x98\x80" "b";
    TextIndex t(s);
    CHECK_EQ(t.size(), 4);
    CHECK_EQ(t.byte_size(), 8);
    CHECK_EQ(t.utf16_size(), 5);

    CHECK_EQ(t.byte_from_cp(2), 3);
    CHECK_EQ(t.byte_from_cp(3), 7);
    CHECK_EQ(t.utf16_from_cp(3), 4);
    CHECK_EQ(t.cp_from_byte(3), 2);
    CHECK_EQ(t.cp_from_byte(5), 2);  // inside the emoji rounds down
    CHECK_EQ(t.cp_from_byte(8), 4);
    CHECK_EQ(t.cp_from_byte(99), 4);
    CHECK_EQ(t.cp_from_byte(-3), 0);
    CHECK_EQ(t.cp_from_utf16(3), 2);  // between the surrogates rounds down
    CHECK_EQ(t.cp_from_utf16(4), 3);
    CHECK_EQ(t.slice(2, 3), "\xF0\x9F\x98\x80");
    CHECK(t.slice_utf16(2, 3) == std::u16string(u"\U0001F600"));
    CHECK_EQ(broa11y::text::utf16_to_utf8(t.slice_utf16(0, 4)), s);

    // Invalid bytes: one U+FFFD each, true byte offsets kept.
    TextIndex bad(std::string("x\xFF\xC3y"));
    CHECK_EQ(bad.size(), 4);
    CHECK_EQ(bad.byte_from_cp(3), 3);
    CHECK_EQ(bad.slice(1, 2), "\xEF\xBF\xBD");
    // A lone surrogate from UTF-16 also becomes U+FFFD.
    CHECK_EQ(broa11y::text::utf16_to_utf8(std::u16string(1, char16_t(0xD800))), "\xEF\xBF\xBD");

    // Units.
    TextIndex w("ab, c\xC3\xA9 d\n\nnext para\n");
    auto word = unit_at(w, 4, TextGranularity::Word);
    CHECK_EQ(word.first, 4);
    CHECK_EQ(word.second, 6);
    auto sep = unit_at(w, 2, TextGranularity::Word);  // ", "
    CHECK_EQ(sep.first, 2);
    CHECK_EQ(sep.second, 4);
    auto nl = unit_at(w, 8, TextGranularity::Word);
    CHECK_EQ(nl.first, 8);
    CHECK_EQ(nl.second, 9);
    auto line = unit_at(w, 3, TextGranularity::Line);
    CHECK_EQ(line.first, 0);
    CHECK_EQ(line.second, 9);
    auto blank = unit_at(w, 9, TextGranularity::Line);
    CHECK_EQ(blank.first, 9);
    CHECK_EQ(blank.second, 10);
    auto para1 = unit_at(w, 1, TextGranularity::Paragraph);
    CHECK_EQ(para1.first, 0);
    CHECK_EQ(para1.second, 10);
    auto para2 = unit_at(w, 12, TextGranularity::Paragraph);
    CHECK_EQ(para2.first, 10);
    CHECK_EQ(para2.second, w.size());
    auto end = unit_at(w, w.size(), TextGranularity::Character);  // past the end: last unit
    CHECK_EQ(end.first, w.size() - 1);
    auto doc = unit_at(w, 5, TextGranularity::Document);
    CHECK_EQ(doc.first, 0);
    CHECK_EQ(doc.second, w.size());
    auto empty = unit_at(TextIndex(""), 0, TextGranularity::Word);
    CHECK_EQ(empty.first, 0);
    CHECK_EQ(empty.second, 0);

    // CJK runs stay one word.
    TextIndex cjk("\xE6\x97\xA5\xE6\x9C\xAC \xE8\xAA\x9E");
    auto jw = unit_at(cjk, 1, TextGranularity::Word);
    CHECK_EQ(jw.first, 0);
    CHECK_EQ(jw.second, 2);

    return bstest::finish("test_text_index");
}
