// UIA Text pattern: ITextProvider on NodeProvider plus the ITextRangeProvider
// it hands out. Ranges count UTF-16 code units, as UIA does; units (character,
// word, line, paragraph, document) come from text::unit_at, so a character is
// a whole code point (a surrogate pair is one character) and the boundaries
// match what AT-SPI and NSAccessibility report.
//
// The model has no per-glyph geometry: a range's bounding rectangle is its
// node's, and RangeFromPoint answers the start of the text.
#include "win/uia_provider.h"
#include "common/text_util.h"

#include <algorithm>
#include <string>

namespace broa11y::uia {

namespace {

// {6d5c9b07-3f1e-4a57-9a52-6f0b7c3d2a11}: lets a range recognise its own kind
// among the ITextRangeProvider pointers UIA hands back.
const IID IID_BroTextRange = {0x6d5c9b07, 0x3f1e, 0x4a57, {0x9a, 0x52, 0x6f, 0x0b, 0x7c, 0x3d, 0x2a, 0x11}};

TextGranularity to_granularity(TextUnit unit) {
    switch (unit) {
        case TextUnit_Character: return TextGranularity::Character;
        case TextUnit_Word: return TextGranularity::Word;
        case TextUnit_Line: return TextGranularity::Line;
        case TextUnit_Paragraph: return TextGranularity::Paragraph;
        // One format run and one page: the whole text.
        case TextUnit_Format:
        case TextUnit_Page:
        case TextUnit_Document:
        default: return TextGranularity::Document;
    }
}

// The start of the unit after the one containing cp, or -1 at the end.
int32_t next_start(const text::TextIndex& t, int32_t cp, TextGranularity g) {
    if (cp >= t.size()) return -1;
    return text::unit_at(t, cp, g).second;
}

// The start of the unit containing cp when cp is inside it, else of the unit
// before; -1 at the start.
int32_t prev_start(const text::TextIndex& t, int32_t cp, TextGranularity g) {
    if (cp <= 0) return -1;
    int32_t s = text::unit_at(t, cp, g).first;
    if (s < cp) return s;
    return text::unit_at(t, cp - 1, g).first;
}

// Moves a character position by count units; returns how many it moved.
int move_by(const text::TextIndex& t, int32_t& cp, TextGranularity g, int count) {
    int moved = 0;
    while (moved < count) {
        int32_t np = next_start(t, cp, g);
        if (np < 0) break;
        cp = np;
        ++moved;
    }
    while (moved > count) {
        int32_t np = prev_start(t, cp, g);
        if (np < 0) break;
        cp = np;
        --moved;
    }
    return moved;
}

class RangeProvider final : public ITextRangeProvider {
public:
    RangeProvider(NodeProvider* owner, int32_t start, int32_t end) : owner_(owner), start_(start), end_(end) {
        owner_->AddRef();
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(ITextRangeProvider) || riid == IID_BroTextRange) {
            *out = static_cast<ITextRangeProvider*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG n = --refs_;
        if (n == 0) delete this;
        return n;
    }

    IFACEMETHODIMP Clone(ITextRangeProvider** out) override {
        if (!out) return E_POINTER;
        *out = new RangeProvider(owner_, start_, end_);
        return S_OK;
    }

    IFACEMETHODIMP Compare(ITextRangeProvider* other, BOOL* out) override {
        if (!out) return E_POINTER;
        RangeProvider*o = unwrap(other);
        if (!o) return E_INVALIDARG;
        *out = o->owner_ == owner_ && o->start_ == start_ && o->end_ == end_;
        return S_OK;
    }

    IFACEMETHODIMP CompareEndpoints(TextPatternRangeEndpoint endpoint, ITextRangeProvider* target,
                                    TextPatternRangeEndpoint target_endpoint, int* out) override {
        if (!out) return E_POINTER;
        RangeProvider*o = unwrap(target);
        if (!o || o->owner_ != owner_) return E_INVALIDARG;
        *out = point(endpoint) - o->point(target_endpoint);
        return S_OK;
    }

    IFACEMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override {
        text::TextIndex t;
        if (!load(t)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (t.size() == 0) {
            start_ = end_ = 0;
            return S_OK;
        }
        auto [s, e] = text::unit_at(t, t.cp_from_utf16(start_), to_granularity(unit));
        start_ = t.utf16_from_cp(s);
        end_ = t.utf16_from_cp(e);
        return S_OK;
    }

    IFACEMETHODIMP FindAttribute(TEXTATTRIBUTEID, VARIANT, BOOL, ITextRangeProvider** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;  // the model carries no text attributes to match
        return S_OK;
    }

    IFACEMETHODIMP FindText(BSTR needle, BOOL backward, BOOL ignore_case, ITextRangeProvider** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        text::TextIndex t;
        if (!load(t)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!needle) return E_INVALIDARG;
        std::u16string hay = t.slice_utf16(t.cp_from_utf16(start_), t.cp_from_utf16(end_));
        std::u16string pin(reinterpret_cast<const char16_t*>(needle), SysStringLen(needle));
        if (pin.empty()) return E_INVALIDARG;
        if (ignore_case) {
            auto upper = [](std::u16string& s) {
                if (!s.empty()) CharUpperBuffW(reinterpret_cast<LPWSTR>(s.data()), static_cast<DWORD>(s.size()));
            };
            upper(hay);
            upper(pin);
        }
        size_t at = backward ? hay.rfind(pin) : hay.find(pin);
        if (at == std::u16string::npos) return S_OK;
        int32_t base = t.utf16_from_cp(t.cp_from_utf16(start_));
        *out = new RangeProvider(owner_, base + static_cast<int32_t>(at),
                             base + static_cast<int32_t>(at + pin.size()));
        return S_OK;
    }

    IFACEMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT* out) override {
        if (!out) return E_POINTER;
        VariantInit(out);
        Node* n = owner_->node();
        if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
        if (attr == UIA_IsReadOnlyAttributeId) {
            out->vt = VT_BOOL;
            out->boolVal = n->has_state(State::ReadOnly) || n->role() == Role::Terminal ? VARIANT_TRUE : VARIANT_FALSE;
            return S_OK;
        }
        out->vt = VT_UNKNOWN;
        return UiaGetReservedNotSupportedValue(&out->punkVal);
    }

    IFACEMETHODIMP GetBoundingRectangles(SAFEARRAY** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        Node* n = owner_->node();
        if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
        const bool any = start_ != end_ && !n->bounds().is_empty();
        SAFEARRAY* sa = SafeArrayCreateVector(VT_R8, 0, any ? 4 : 0);
        if (!sa) return E_OUTOFMEMORY;
        if (any) {
            RECT r = to_screen(*owner_->context(), n->bounds());
            double v[4] = {static_cast<double>(r.left), static_cast<double>(r.top),
                           static_cast<double>(r.right - r.left), static_cast<double>(r.bottom - r.top)};
            for (LONG i = 0; i < 4; ++i) SafeArrayPutElement(sa, &i, &v[i]);
        }
        *out = sa;
        return S_OK;
    }

    IFACEMETHODIMP GetEnclosingElement(IRawElementProviderSimple** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!owner_->node()) return UIA_E_ELEMENTNOTAVAILABLE;
        owner_->AddRef();
        *out = static_cast<IRawElementProviderSimple*>(owner_);
        return S_OK;
    }

    IFACEMETHODIMP GetText(int max_length, BSTR* out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        text::TextIndex t;
        if (!load(t)) return UIA_E_ELEMENTNOTAVAILABLE;
        std::u16string s = t.slice_utf16(t.cp_from_utf16(start_), t.cp_from_utf16(end_));
        if (max_length >= 0 && s.size() > static_cast<size_t>(max_length)) s.resize(static_cast<size_t>(max_length));
        *out = SysAllocStringLen(reinterpret_cast<const OLECHAR*>(s.data()), static_cast<UINT>(s.size()));
        return S_OK;
    }

    IFACEMETHODIMP Move(TextUnit unit, int count, int* out) override {
        if (!out) return E_POINTER;
        *out = 0;
        text::TextIndex t;
        if (!load(t)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (count == 0) return S_OK;
        const TextGranularity g = to_granularity(unit);
        int32_t s = t.cp_from_utf16(start_);
        const bool degenerate = start_ == end_;
        if (!degenerate && t.size() > 0 && s < t.size()) s = text::unit_at(t, s, g).first;
        int32_t pos = s;
        int moved = move_by(t, pos, g, count);
        // A range that spans text keeps spanning one unit, so it cannot land
        // on the empty position past the end.
        if (!degenerate && pos >= t.size() && moved > 0) {
            pos = prev_start(t, pos, g);
            --moved;
        }
        if (degenerate || t.size() == 0) {
            start_ = end_ = t.utf16_from_cp(pos);
        } else {
            auto [us, ue] = text::unit_at(t, pos, g);
            start_ = t.utf16_from_cp(us);
            end_ = t.utf16_from_cp(ue);
        }
        *out = moved;
        return S_OK;
    }

    IFACEMETHODIMP MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count,
                                      int* out) override {
        if (!out) return E_POINTER;
        *out = 0;
        text::TextIndex t;
        if (!load(t)) return UIA_E_ELEMENTNOTAVAILABLE;
        int32_t pos = t.cp_from_utf16(point(endpoint));
        *out = move_by(t, pos, to_granularity(unit), count);
        set_point(endpoint, t.utf16_from_cp(pos));
        return S_OK;
    }

    IFACEMETHODIMP MoveEndpointByRange(TextPatternRangeEndpoint endpoint, ITextRangeProvider* target,
                                       TextPatternRangeEndpoint target_endpoint) override {
        RangeProvider*o = unwrap(target);
        if (!o || o->owner_ != owner_) return E_INVALIDARG;
        set_point(endpoint, o->point(target_endpoint));
        return S_OK;
    }

    IFACEMETHODIMP Select() override {
        Node* n = owner_->node();
        if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
        text::TextIndex t(n->text());
        ActionParams params;
        params.range_val = broa11y::TextRange{t.byte_from_cp(t.cp_from_utf16(start_)), t.byte_from_cp(t.cp_from_utf16(end_))};
        return n->perform_action(kActionSetSelection, params) ? S_OK : UIA_E_INVALIDOPERATION;
    }

    IFACEMETHODIMP AddToSelection() override { return UIA_E_INVALIDOPERATION; }      // single selection only
    IFACEMETHODIMP RemoveFromSelection() override { return UIA_E_INVALIDOPERATION; }
    IFACEMETHODIMP ScrollIntoView(BOOL) override { return owner_->node() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE; }

    IFACEMETHODIMP GetChildren(SAFEARRAY** out) override {
        if (!out) return E_POINTER;
        *out = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
        return *out ? S_OK : E_OUTOFMEMORY;
    }

private:
    ~RangeProvider() { owner_->Release(); }

    static RangeProvider*unwrap(ITextRangeProvider* p) {
        if (!p) return nullptr;
        void* raw = nullptr;
        if (FAILED(p->QueryInterface(IID_BroTextRange, &raw)) || !raw) return nullptr;
        auto* r = static_cast<RangeProvider*>(static_cast<ITextRangeProvider*>(raw));
        r->Release();  // the caller still holds p
        return r;
    }

    // Reads the node's current text and clamps the endpoints to it.
    bool load(text::TextIndex& t) {
        Node* n = owner_->node();
        if (!n) return false;
        t = text::TextIndex(n->text());
        start_ = t.utf16_from_cp(t.cp_from_utf16(start_));
        end_ = t.utf16_from_cp(t.cp_from_utf16(end_));
        if (end_ < start_) end_ = start_;
        return true;
    }

    [[nodiscard]] int32_t point(TextPatternRangeEndpoint e) const {
        return e == TextPatternRangeEndpoint_Start ? start_ : end_;
    }

    void set_point(TextPatternRangeEndpoint e, int32_t v) {
        if (e == TextPatternRangeEndpoint_Start) {
            start_ = v;
            if (end_ < start_) end_ = start_;
        } else {
            end_ = v;
            if (start_ > end_) start_ = end_;
        }
    }

    std::atomic<ULONG> refs_{1};
    NodeProvider* owner_;
    int32_t start_;
    int32_t end_;
};

SAFEARRAY* range_array(std::initializer_list<ITextRangeProvider*> ranges) {
    SAFEARRAY* sa = SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(ranges.size()));
    if (!sa) return nullptr;
    LONG i = 0;
    for (ITextRangeProvider* r : ranges) {
        SafeArrayPutElement(sa, &i, r);  // AddRefs
        r->Release();
        ++i;
    }
    return sa;
}

} // namespace

ITextRangeProvider* make_text_range(NodeProvider* owner, int32_t start, int32_t end) {
    return new RangeProvider(owner, start, end);
}

// ── ITextProvider on NodeProvider ────────────────────────────────────────────

IFACEMETHODIMP NodeProvider::GetSelection(SAFEARRAY** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    text::TextIndex t(n->text());
    const broa11y::TextRange& sel = n->selection();
    if (!sel.is_empty()) {
        *out = range_array({make_text_range(this, t.utf16_from_cp(t.cp_from_byte(sel.start_offset)),
                                            t.utf16_from_cp(t.cp_from_byte(sel.end_offset)))});
    } else if (n->caret_offset() >= 0) {
        int32_t c = t.utf16_from_cp(t.cp_from_byte(n->caret_offset()));
        *out = range_array({make_text_range(this, c, c)});
    } else {
        *out = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
    }
    return *out ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP NodeProvider::GetVisibleRanges(SAFEARRAY** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    text::TextIndex t(n->text());
    *out = range_array({make_text_range(this, 0, t.utf16_size())});
    return *out ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP NodeProvider::RangeFromChild(IRawElementProviderSimple*, ITextRangeProvider** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    return node() ? E_INVALIDARG : UIA_E_ELEMENTNOTAVAILABLE;  // text nodes have no embedded children
}

IFACEMETHODIMP NodeProvider::RangeFromPoint(UiaPoint, ITextRangeProvider** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!node()) return UIA_E_ELEMENTNOTAVAILABLE;
    *out = make_text_range(this, 0, 0);
    return S_OK;
}

IFACEMETHODIMP NodeProvider::get_DocumentRange(ITextRangeProvider** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    Node* n = node();
    if (!n) return UIA_E_ELEMENTNOTAVAILABLE;
    *out = make_text_range(this, 0, text::TextIndex(n->text()).utf16_size());
    return S_OK;
}

IFACEMETHODIMP NodeProvider::get_SupportedTextSelection(SupportedTextSelection* out) {
    if (!out) return E_POINTER;
    *out = SupportedTextSelection_Single;
    return S_OK;
}

} // namespace broa11y::uia
