// org.a11y.atspi.Text and org.a11y.atspi.EditableText.
//
// Offsets on the wire are characters; the model's caret and selection are
// UTF-8 byte offsets, converted with text::TextIndex. Units come from
// text::unit_at, the same boundaries UIA and NSAccessibility report. The model
// has no per-glyph geometry: character and range extents are the node's, and
// GetOffsetAtPoint answers -1.
#include "linux/atspi_util.h"
#include "common/text_util.h"

#include <algorithm>
#include <string>
#include <tuple>

namespace broa11y::atspi {

namespace {

// AtspiTextGranularity (GetStringAtOffset). No sentence segmentation: a
// sentence is answered as its line.
TextGranularity from_granularity(uint32_t g) {
    switch (g) {
        case 0: return TextGranularity::Character;
        case 1: return TextGranularity::Word;
        case 2: return TextGranularity::Line;
        case 3: return TextGranularity::Line;
        case 4: return TextGranularity::Paragraph;
        default: return TextGranularity::Character;
    }
}

// AtspiTextBoundaryType (the older Get*Offset calls).
TextGranularity from_boundary(uint32_t b) {
    switch (b) {
        case 0: return TextGranularity::Character;
        case 1:
        case 2: return TextGranularity::Word;
        default: return TextGranularity::Line;  // sentence and line boundaries
    }
}

int reply_unit(sd_bus_message* m, const text::TextIndex& t, int32_t s, int32_t e) {
    std::string str = t.slice(s, e);
    return sd_bus_reply_method_return(m, "sii", str.c_str(), s, e);
}

// The unit at, before (-1) or after (+1) character `off`.
int text_unit(sd_bus_message* m, void* u, sd_bus_error* err, bool boundary_api, int direction) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t off = 0;
    uint32_t kind = 0;
    int r = sd_bus_message_read(m, "iu", &off, &kind);
    if (r < 0) return r;
    text::TextIndex t(n->text());
    if (t.size() == 0 || off < 0 || off > t.size()) return reply_unit(m, t, 0, 0);
    const TextGranularity g = boundary_api ? from_boundary(kind) : from_granularity(kind);
    auto [s, e] = text::unit_at(t, off, g);
    if (direction < 0) {
        if (s == 0) return reply_unit(m, t, 0, 0);
        std::tie(s, e) = text::unit_at(t, s - 1, g);
    } else if (direction > 0) {
        if (e >= t.size()) return reply_unit(m, t, t.size(), t.size());
        std::tie(s, e) = text::unit_at(t, e, g);
    }
    return reply_unit(m, t, s, e);
}

int m_get_string_at_offset(sd_bus_message* m, void* u, sd_bus_error* err) { return text_unit(m, u, err, false, 0); }
int m_get_text_at_offset(sd_bus_message* m, void* u, sd_bus_error* err) { return text_unit(m, u, err, true, 0); }
int m_get_text_before_offset(sd_bus_message* m, void* u, sd_bus_error* err) { return text_unit(m, u, err, true, -1); }
int m_get_text_after_offset(sd_bus_message* m, void* u, sd_bus_error* err) { return text_unit(m, u, err, true, 1); }

int prop_character_count(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                         sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    return sd_bus_message_append(reply, "i", text::TextIndex(n->text()).size());
}

int prop_caret_offset(sd_bus*, const char* path, const char*, const char*, sd_bus_message* reply, void* u,
                      sd_bus_error* err) {
    Node* n = node_at(srv(u), path, err);
    if (!n) return -ENOENT;
    int32_t caret = n->caret_offset() < 0 ? -1 : text::TextIndex(n->text()).cp_from_byte(n->caret_offset());
    return sd_bus_message_append(reply, "i", caret);
}

int m_get_text(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t s = 0, e = 0;
    int r = sd_bus_message_read(m, "ii", &s, &e);
    if (r < 0) return r;
    text::TextIndex t(n->text());
    if (e < 0 || e > t.size()) e = t.size();  // -1: to the end
    std::string str = t.slice(std::max(s, 0), e);
    return sd_bus_reply_method_return(m, "s", str.c_str());
}

// Asks the application to select [s, e) (characters); s == e moves the caret.
bool request_selection(Node* n, int32_t s, int32_t e) {
    text::TextIndex t(n->text());
    if (s < 0 || e < 0 || s > t.size() || e > t.size()) return false;
    if (e < s) std::swap(s, e);
    ActionParams p;
    p.range_val = TextRange{t.byte_from_cp(s), t.byte_from_cp(e)};
    return n->perform_action(kActionSetSelection, p);
}

int m_set_caret_offset(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t off = 0;
    int r = sd_bus_message_read(m, "i", &off);
    if (r < 0) return r;
    return reply_bool(m, request_selection(n, off, off));
}

int m_get_character_at_offset(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t off = 0;
    int r = sd_bus_message_read(m, "i", &off);
    if (r < 0) return r;
    text::TextIndex t(n->text());
    int32_t c = off >= 0 && off < t.size() ? static_cast<int32_t>(t.at(off)) : 0;
    return sd_bus_reply_method_return(m, "i", c);
}

// No text attributes in the model: empty sets, one run over the whole text.
int m_get_attribute_value(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "s", "");
}

int reply_attribute_run(sd_bus_message* m, Node* n) {
    Reply rep(m);
    if (rep.r < 0) return rep.r;
    sd_bus_message_append(rep.msg, "a{ss}", 0);
    rep.r = sd_bus_message_append(rep.msg, "ii", 0, text::TextIndex(n->text()).size());
    return rep.send();
}

int m_get_attributes(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return reply_attribute_run(m, n);
}

int m_get_default_attributes(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "a{ss}", 0);
}

int m_node_extents(sd_bus_message* m, void* u, sd_bus_error* err, uint32_t coord_type) {
    Server* s = srv(u);
    Node* n = node_at(s, path_of_msg(m), err);
    if (!n) return -ENOENT;
    RectF b = rect_in(s, n, coord_type);
    return sd_bus_reply_method_return(m, "iiii", ri(b.x), ri(b.y), ri(b.width), ri(b.height));
}

int m_get_character_extents(sd_bus_message* m, void* u, sd_bus_error* err) {
    int32_t off = 0;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "iu", &off, &ct);
    if (r < 0) return r;
    return m_node_extents(m, u, err, ct);
}

int m_get_range_extents(sd_bus_message* m, void* u, sd_bus_error* err) {
    int32_t s = 0, e = 0;
    uint32_t ct = 0;
    int r = sd_bus_message_read(m, "iiu", &s, &e, &ct);
    if (r < 0) return r;
    return m_node_extents(m, u, err, ct);
}

int m_get_offset_at_point(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "i", -1);
}

int m_get_n_selections(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return sd_bus_reply_method_return(m, "i", n->selection().is_empty() ? 0 : 1);
}

int m_get_selection(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t i = 0;
    int r = sd_bus_message_read(m, "i", &i);
    if (r < 0) return r;
    if (i != 0 || n->selection().is_empty()) return sd_bus_reply_method_return(m, "ii", 0, 0);
    text::TextIndex t(n->text());
    return sd_bus_reply_method_return(m, "ii", t.cp_from_byte(n->selection().start_offset),
                                      t.cp_from_byte(n->selection().end_offset));
}

int m_add_selection(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t s = 0, e = 0;
    int r = sd_bus_message_read(m, "ii", &s, &e);
    if (r < 0) return r;
    // One selection only: adding one while another exists is refused.
    if (!n->selection().is_empty()) return reply_bool(m, false);
    return reply_bool(m, request_selection(n, s, e));
}

int m_set_selection(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t i = 0, s = 0, e = 0;
    int r = sd_bus_message_read(m, "iii", &i, &s, &e);
    if (r < 0) return r;
    return reply_bool(m, i == 0 && request_selection(n, s, e));
}

int m_remove_selection(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t i = 0;
    int r = sd_bus_message_read(m, "i", &i);
    if (r < 0) return r;
    if (i != 0 || n->selection().is_empty()) return reply_bool(m, false);
    text::TextIndex t(n->text());
    int32_t caret = n->caret_offset() < 0 ? 0 : t.cp_from_byte(n->caret_offset());
    return reply_bool(m, request_selection(n, caret, caret));
}

int m_get_bounded_ranges(sd_bus_message* m, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(m, "a(iisv)", 0);
}

int m_get_attribute_run(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    return reply_attribute_run(m, n);
}

int m_refuse_scroll(sd_bus_message* m, void*, sd_bus_error*) {
    return reply_bool(m, false);
}

}  // namespace

extern const sd_bus_vtable kTextVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("CharacterCount", "i", prop_character_count, 0, 0),
    SD_BUS_PROPERTY("CaretOffset", "i", prop_caret_offset, 0, 0),
    SD_BUS_METHOD("GetStringAtOffset", "iu", "sii", m_get_string_at_offset, 0),
    SD_BUS_METHOD("GetText", "ii", "s", m_get_text, 0),
    SD_BUS_METHOD("SetCaretOffset", "i", "b", m_set_caret_offset, 0),
    SD_BUS_METHOD("GetTextBeforeOffset", "iu", "sii", m_get_text_before_offset, 0),
    SD_BUS_METHOD("GetTextAtOffset", "iu", "sii", m_get_text_at_offset, 0),
    SD_BUS_METHOD("GetTextAfterOffset", "iu", "sii", m_get_text_after_offset, 0),
    SD_BUS_METHOD("GetCharacterAtOffset", "i", "i", m_get_character_at_offset, 0),
    SD_BUS_METHOD("GetAttributeValue", "is", "s", m_get_attribute_value, 0),
    SD_BUS_METHOD("GetAttributes", "i", "a{ss}ii", m_get_attributes, 0),
    SD_BUS_METHOD("GetDefaultAttributes", "", "a{ss}", m_get_default_attributes, 0),
    SD_BUS_METHOD("GetCharacterExtents", "iu", "iiii", m_get_character_extents, 0),
    SD_BUS_METHOD("GetOffsetAtPoint", "iiu", "i", m_get_offset_at_point, 0),
    SD_BUS_METHOD("GetNSelections", "", "i", m_get_n_selections, 0),
    SD_BUS_METHOD("GetSelection", "i", "ii", m_get_selection, 0),
    SD_BUS_METHOD("AddSelection", "ii", "b", m_add_selection, 0),
    SD_BUS_METHOD("RemoveSelection", "i", "b", m_remove_selection, 0),
    SD_BUS_METHOD("SetSelection", "iii", "b", m_set_selection, 0),
    SD_BUS_METHOD("GetRangeExtents", "iiu", "iiii", m_get_range_extents, 0),
    SD_BUS_METHOD("GetBoundedRanges", "iiiiuuu", "a(iisv)", m_get_bounded_ranges, 0),
    SD_BUS_METHOD("GetAttributeRun", "ib", "a{ss}ii", m_get_attribute_run, 0),
    SD_BUS_METHOD("GetDefaultAttributeSet", "", "a{ss}", m_get_default_attributes, 0),
    SD_BUS_METHOD("ScrollSubstringTo", "iiu", "b", m_refuse_scroll, 0),
    SD_BUS_METHOD("ScrollSubstringToPoint", "iiuii", "b", m_refuse_scroll, 0),
    SD_BUS_VTABLE_END,
};

// ── EditableText: every edit is a set_value request with the whole new text ──

namespace {

bool request_text(Node* n, std::string new_text) {
    ActionParams p;
    p.string_val = std::move(new_text);
    return n->perform_action(kActionSetValue, p);
}

int m_set_text_contents(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    const char* s = nullptr;
    int r = sd_bus_message_read(m, "s", &s);
    if (r < 0) return r;
    return reply_bool(m, request_text(n, s ? s : ""));
}

int m_insert_text(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t pos = 0, len = 0;
    const char* s = nullptr;
    int r = sd_bus_message_read(m, "isi", &pos, &s, &len);
    if (r < 0) return r;
    text::TextIndex t(n->text());
    text::TextIndex ins(s ? s : "");
    if (pos < 0 || pos > t.size()) return reply_bool(m, false);
    if (len < 0 || len > ins.size()) len = ins.size();  // length counts characters of the inserted text
    std::string out = t.slice(0, pos) + ins.slice(0, len) + t.slice(pos, t.size());
    return reply_bool(m, request_text(n, std::move(out)));
}

int m_delete_text(sd_bus_message* m, void* u, sd_bus_error* err) {
    Node* n = node_at(srv(u), path_of_msg(m), err);
    if (!n) return -ENOENT;
    int32_t s = 0, e = 0;
    int r = sd_bus_message_read(m, "ii", &s, &e);
    if (r < 0) return r;
    text::TextIndex t(n->text());
    if (s < 0 || e < s || s > t.size()) return reply_bool(m, false);
    e = std::min(e, t.size());
    return reply_bool(m, request_text(n, t.slice(0, s) + t.slice(e, t.size())));
}

int m_copy_text(sd_bus_message* m, void*, sd_bus_error*) {
    // There is no clipboard in the model; the application owns its own.
    return sd_bus_reply_method_return(m, "");
}

int m_refuse_clipboard(sd_bus_message* m, void*, sd_bus_error*) {
    return reply_bool(m, false);
}

}  // namespace

extern const sd_bus_vtable kEditableTextVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("SetTextContents", "s", "b", m_set_text_contents, 0),
    SD_BUS_METHOD("InsertText", "isi", "b", m_insert_text, 0),
    SD_BUS_METHOD("CopyText", "ii", "", m_copy_text, 0),
    SD_BUS_METHOD("CutText", "ii", "b", m_refuse_clipboard, 0),
    SD_BUS_METHOD("DeleteText", "ii", "b", m_delete_text, 0),
    SD_BUS_METHOD("PasteText", "i", "b", m_refuse_clipboard, 0),
    SD_BUS_VTABLE_END,
};

} // namespace broa11y::atspi
