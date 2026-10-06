// The assistive-technology side of test_win_uia: a separate process using the
// UI Automation client API (IUIAutomation) on the bridge's window. Nothing in
// here knows broa11y; it sees only what UIA reports.
#include "check.h"

#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>

namespace {

using namespace std::chrono_literals;

// Receives the events the client subscribes to.
class Events final : public IUIAutomationPropertyChangedEventHandler,
                     public IUIAutomationNotificationEventHandler,
                     public IUIAutomationStructureChangedEventHandler {
public:
    std::atomic<int> name_changes{0};
    std::atomic<int> notifications{0};
    std::atomic<int> structure_changes{0};
    std::wstring last_name;          // written before name_changes is bumped
    std::wstring last_notification;  // written before notifications is bumped

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IUIAutomationPropertyChangedEventHandler)) {
            *out = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
        } else if (riid == __uuidof(IUIAutomationNotificationEventHandler)) {
            *out = static_cast<IUIAutomationNotificationEventHandler*>(this);
        } else if (riid == __uuidof(IUIAutomationStructureChangedEventHandler)) {
            *out = static_cast<IUIAutomationStructureChangedEventHandler*>(this);
        } else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    IFACEMETHODIMP_(ULONG) Release() override { return --refs_; }  // lives on the stack

    IFACEMETHODIMP HandlePropertyChangedEvent(IUIAutomationElement*, PROPERTYID id, VARIANT v) override {
        if (id == UIA_NamePropertyId && v.vt == VT_BSTR) {
            last_name = v.bstrVal;
            ++name_changes;
        }
        return S_OK;
    }
    IFACEMETHODIMP HandleNotificationEvent(IUIAutomationElement*, NotificationKind, NotificationProcessing,
                                           BSTR text, BSTR) override {
        last_notification = text ? text : L"";
        ++notifications;
        return S_OK;
    }
    IFACEMETHODIMP HandleStructureChangedEvent(IUIAutomationElement*, StructureChangeType, SAFEARRAY*) override {
        ++structure_changes;
        return S_OK;
    }

private:
    std::atomic<ULONG> refs_{1};
};

template <class T>
struct Ptr {
    T* p = nullptr;
    Ptr() = default;
    Ptr(const Ptr&) = delete;
    Ptr& operator=(const Ptr&) = delete;
    ~Ptr() {
        if (p) p->Release();
    }
    T** put() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

std::wstring bstr(BSTR b) {
    std::wstring s = b ? std::wstring(b, SysStringLen(b)) : std::wstring();
    SysFreeString(b);
    return s;
}

std::wstring name_of(IUIAutomationElement* e) {
    BSTR b = nullptr;
    e->get_CurrentName(&b);
    return bstr(b);
}

std::wstring text_of(IUIAutomationTextRange* r) {
    BSTR b = nullptr;
    r->GetText(-1, &b);
    return bstr(b);
}

bool same(const std::wstring& a, const wchar_t* b) { return a == b; }

}  // namespace

int run_client(HWND hwnd) {
    REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    int rc = 0;
    {
        Ptr<IUIAutomation> uia;
        REQUIRE(SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER,
                                           __uuidof(IUIAutomation), reinterpret_cast<void**>(uia.put()))));

        Ptr<IUIAutomationElement> root;
        REQUIRE(SUCCEEDED(uia->ElementFromHandle(hwnd, root.put())) && root);
        // The provider's name wins over the window title it is hosted in.
        CHECK(same(name_of(root.p), L"broa11y test window"));

        Ptr<IUIAutomationCondition> all;
        uia->CreateTrueCondition(all.put());
        Ptr<IUIAutomationElementArray> kids;
        REQUIRE(SUCCEEDED(root->FindAll(TreeScope_Children, all.p, kids.put())) && kids);
        int count = 0;
        kids->get_Length(&count);
        REQUIRE(count == 5);

        const wchar_t* names[] = {L"Submit", L"Wrap lines", L"Command", L"Volume", L"Status"};
        const CONTROLTYPEID types[] = {UIA_ButtonControlTypeId, UIA_CheckBoxControlTypeId, UIA_EditControlTypeId,
                                       UIA_SliderControlTypeId, UIA_TextControlTypeId};
        IUIAutomationElement* el[5] = {};
        for (int i = 0; i < 5; ++i) {
            kids->GetElement(i, &el[i]);
            REQUIRE(el[i] != nullptr);
            CHECK(same(name_of(el[i]), names[i]));
            CONTROLTYPEID ct = 0;
            el[i]->get_CurrentControlType(&ct);
            CHECK_EQ(ct, types[i]);
            BSTR id = nullptr;
            el[i]->get_CurrentAutomationId(&id);
            CHECK(bstr(id) == std::to_wstring(i + 2));
        }
        IUIAutomationElement* button = el[0];
        IUIAutomationElement* check = el[1];
        IUIAutomationElement* entry = el[2];
        IUIAutomationElement* slider = el[3];

        // Bounds: client pixels of a popup at (100, 100) on the screen.
        RECT r{};
        button->get_CurrentBoundingRectangle(&r);
        CHECK_EQ(r.left, 110L);
        CHECK_EQ(r.top, 110L);
        CHECK_EQ(r.right, 210L);
        CHECK_EQ(r.bottom, 140L);

        // Navigation back up.
        Ptr<IUIAutomationTreeWalker> walker;
        uia->get_RawViewWalker(walker.put());
        Ptr<IUIAutomationElement> parent;
        walker->GetParentElement(entry, parent.put());
        REQUIRE(parent);
        CHECK(same(name_of(parent.p), L"broa11y test window"));
        Ptr<IUIAutomationElement> next;
        walker->GetNextSiblingElement(button, next.put());
        REQUIRE(next);
        CHECK(same(name_of(next.p), L"Wrap lines"));

        // ── Text pattern: UTF-16 units over the model's UTF-8 ──
        const wchar_t* original = L"héllo wörld \U0001F600";
        {
            Ptr<IUIAutomationTextPattern> text;
            REQUIRE(SUCCEEDED(entry->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern),
                                                         reinterpret_cast<void**>(text.put()))) && text);
            Ptr<IUIAutomationTextRange> doc;
            text->get_DocumentRange(doc.put());
            REQUIRE(doc);
            CHECK(same(text_of(doc.p), original));

            Ptr<IUIAutomationTextRangeArray> sel;
            text->GetSelection(sel.put());
            REQUIRE(sel);
            int n = 0;
            sel->get_Length(&n);
            REQUIRE(n == 1);
            Ptr<IUIAutomationTextRange> caret;
            sel->GetElement(0, caret.put());
            CHECK(same(text_of(caret.p), L""));  // the caret: a degenerate range before the 'w'
            caret->ExpandToEnclosingUnit(TextUnit_Word);
            CHECK(same(text_of(caret.p), L"wörld"));
            CHECK(SUCCEEDED(caret->Select()));  // reaches the app as set_selection 7..13

            // The emoji is one character and two UTF-16 units.
            Ptr<IUIAutomationTextRange> ch;
            doc->Clone(ch.put());
            int moved = 0;
            ch->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 12, &moved);
            CHECK_EQ(moved, 12);
            CHECK(same(text_of(ch.p), L"\U0001F600"));
            ch->Move(TextUnit_Character, -1, &moved);
            CHECK_EQ(moved, -1);
            CHECK(same(text_of(ch.p), L" "));
            ch->Move(TextUnit_Word, -1, &moved);
            CHECK_EQ(moved, -1);
            CHECK(same(text_of(ch.p), L"wörld"));

            Ptr<IUIAutomationTextRange> found;
            doc->FindText(SysAllocString(L"WÖRLD"), FALSE, TRUE, found.put());
            REQUIRE(found);
            CHECK(same(text_of(found.p), L"wörld"));
        }

        // ── Value pattern reads ──
        {
            Ptr<IUIAutomationValuePattern> value;
            entry->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern),
                                       reinterpret_cast<void**>(value.put()));
            REQUIRE(value);
            BSTR v = nullptr;
            value->get_CurrentValue(&v);
            CHECK(same(bstr(v), original));
        }

        // ── Invoke, with the events it causes ──
        Events events;
        PROPERTYID name_prop = UIA_NamePropertyId;
        CHECK(SUCCEEDED(uia->AddPropertyChangedEventHandlerNativeArray(button, TreeScope_Element, nullptr, &events,
                                                                       &name_prop, 1)));
        CHECK(SUCCEEDED(uia->AddStructureChangedEventHandler(root.p, TreeScope_Subtree, nullptr, &events)));
        Ptr<IUIAutomation5> uia5;
        uia->QueryInterface(__uuidof(IUIAutomation5), reinterpret_cast<void**>(uia5.put()));
        REQUIRE(uia5);
        CHECK(SUCCEEDED(uia5->AddNotificationEventHandler(root.p, TreeScope_Subtree, nullptr, &events)));
        {
            Ptr<IUIAutomationInvokePattern> invoke;
            button->GetCurrentPatternAs(UIA_InvokePatternId, __uuidof(IUIAutomationInvokePattern),
                                        reinterpret_cast<void**>(invoke.put()));
            REQUIRE(invoke);
            CHECK(SUCCEEDED(invoke->Invoke()));
        }
        CHECK(bstest::wait_until([&] { return events.name_changes.load() > 0; }, 5000ms));
        CHECK(events.last_name == L"Submitted");
        CHECK(bstest::wait_until([&] { return events.notifications.load() > 0; }, 5000ms));
        CHECK(events.last_notification == L"Form submitted");
        CHECK(bstest::wait_until([&] { return events.structure_changes.load() > 0; }, 5000ms));
        CHECK(same(name_of(button), L"Submitted"));
        {
            Ptr<IUIAutomationElementArray> after;
            root->FindAll(TreeScope_Children, all.p, after.put());
            REQUIRE(after);
            int n = 0;
            after->get_Length(&n);
            CHECK_EQ(n, 6);
            Ptr<IUIAutomationElement> last;
            if (n == 6) {
                after->GetElement(5, last.put());
                CHECK(same(name_of(last.p), L"Result"));
            }
        }
        uia->RemoveAllEventHandlers();

        // ── Toggle ──
        {
            Ptr<IUIAutomationTogglePattern> toggle;
            check->GetCurrentPatternAs(UIA_TogglePatternId, __uuidof(IUIAutomationTogglePattern),
                                       reinterpret_cast<void**>(toggle.put()));
            REQUIRE(toggle);
            ToggleState s = ToggleState_Indeterminate;
            toggle->get_CurrentToggleState(&s);
            CHECK_EQ(s, ToggleState_Off);
            CHECK(SUCCEEDED(toggle->Toggle()));
            toggle->get_CurrentToggleState(&s);
            CHECK_EQ(s, ToggleState_On);
            // A check box toggles; it does not also claim Invoke.
            Ptr<IUnknown> no_invoke;
            check->GetCurrentPattern(UIA_InvokePatternId, no_invoke.put());
            CHECK(!no_invoke);
        }

        // ── RangeValue ──
        {
            Ptr<IUIAutomationRangeValuePattern> range;
            slider->GetCurrentPatternAs(UIA_RangeValuePatternId, __uuidof(IUIAutomationRangeValuePattern),
                                        reinterpret_cast<void**>(range.put()));
            REQUIRE(range);
            double d = 0;
            range->get_CurrentValue(&d);
            CHECK_EQ(d, 50.0);
            range->get_CurrentMaximum(&d);
            CHECK_EQ(d, 100.0);
            range->get_CurrentSmallChange(&d);
            CHECK_EQ(d, 5.0);
            CHECK(SUCCEEDED(range->SetValue(75.0)));
            range->get_CurrentValue(&d);
            CHECK_EQ(d, 75.0);
            CHECK(FAILED(range->SetValue(150.0)));  // out of range is refused, not clamped
            range->get_CurrentValue(&d);
            CHECK_EQ(d, 75.0);
        }

        // ── Value write ──
        {
            Ptr<IUIAutomationValuePattern> value;
            entry->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern),
                                       reinterpret_cast<void**>(value.put()));
            REQUIRE(value);
            CHECK(SUCCEEDED(value->SetValue(SysAllocString(L"new ✓ text"))));
            BSTR v = nullptr;
            value->get_CurrentValue(&v);
            CHECK(same(bstr(v), L"new ✓ text"));
        }

        // ── Focus ──
        {
            CHECK(SUCCEEDED(button->SetFocus()));
            BOOL has = FALSE;
            button->get_CurrentHasKeyboardFocus(&has);
            CHECK(has);
        }

        for (IUIAutomationElement* e : el) e->Release();
        rc = bstest::finish("test_win_uia (client)");
    }
    CoUninitialize();
    return rc;
}
