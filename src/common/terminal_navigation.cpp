#include "broa11y/terminal.h"
#include "common/text_util.h"

namespace broa11y {

namespace {

// Text at, before or after a UTF-8 byte offset, at the given unit. The units
// are found on characters (text::unit_at), so a multi-byte character is never
// split and every bridge agrees on the boundaries.
std::string unit_text(const std::string& full, int32_t byte_offset, TextGranularity unit,
                      int direction, int32_t* out_start, int32_t* out_end) {
    text::TextIndex t(full);
    auto put = [&](int32_t s, int32_t e) {
        if (out_start) *out_start = t.byte_from_cp(s);
        if (out_end) *out_end = t.byte_from_cp(e);
        return t.slice(s, e);
    };
    if (t.size() == 0) return put(0, 0);

    auto [s, e] = text::unit_at(t, t.cp_from_byte(byte_offset), unit);
    if (direction < 0) {
        if (s == 0) return put(0, 0);
        auto prev = text::unit_at(t, s - 1, unit);
        return put(prev.first, prev.second);
    }
    if (direction > 0) {
        if (e >= t.size()) return put(t.size(), t.size());
        auto next = text::unit_at(t, e, unit);
        return put(next.first, next.second);
    }
    return put(s, e);
}

} // namespace

std::string TerminalAccessibility::get_text_at_offset(int32_t offset,
                                                      TextGranularity granularity,
                                                      int32_t* out_start,
                                                      int32_t* out_end) const {
    rebuild_cache();
    return unit_text(cached_full_text_, offset, granularity, 0, out_start, out_end);
}

std::string TerminalAccessibility::get_text_before_offset(int32_t offset,
                                                         TextGranularity granularity,
                                                         int32_t* out_start,
                                                         int32_t* out_end) const {
    rebuild_cache();
    return unit_text(cached_full_text_, offset, granularity, -1, out_start, out_end);
}

std::string TerminalAccessibility::get_text_after_offset(int32_t offset,
                                                        TextGranularity granularity,
                                                        int32_t* out_start,
                                                        int32_t* out_end) const {
    rebuild_cache();
    return unit_text(cached_full_text_, offset, granularity, 1, out_start, out_end);
}

} // namespace broa11y
