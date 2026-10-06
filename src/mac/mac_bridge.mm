// NSAccessibility provider: an NSAccessibilityElement subclass per node.
//
// Coordinates: node bounds are top-left-origin points in the host view;
// accessibilityFrame is screen coordinates (bottom-left origin), converted
// through the view and its window. Text ranges are UTF-16 (NSString), mapped
// to the model's UTF-8 byte offsets through text::TextIndex.
#include "broa11y/mac_bridge.h"
#include "broa11y/tree.h"
#include "common/text_util.h"

#import <AppKit/AppKit.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

namespace broa11y {
struct MacContext;
}

@interface BroA11yElement : NSAccessibilityElement
- (instancetype)initWithContext:(std::shared_ptr<broa11y::MacContext>)ctx node:(broa11y::NodeId)node;
@property(nonatomic, readonly) broa11y::NodeId nodeId;
@end

namespace broa11y {

struct MacContext {
    Tree* tree = nullptr;  // null once the bridge has shut down
    __weak NSView* view = nil;
    std::unordered_map<NodeId, BroA11yElement*> elements;
    std::weak_ptr<MacContext> weak_self;

    BroA11yElement* element(NodeId id) {
        if (!tree || !tree->contains_node(id)) return nil;
        auto it = elements.find(id);
        if (it != elements.end()) return it->second;
        BroA11yElement* e = [[BroA11yElement alloc] initWithContext:weak_self.lock() node:id];
        elements.emplace(id, e);
        return e;
    }

    // Node bounds (view points, top-left origin) to screen.
    NSRect to_screen(const RectF& r) const {
        NSView* v = view;
        if (!v) return NSZeroRect;
        CGFloat y = v.isFlipped ? r.y : NSHeight(v.bounds) - r.y - r.height;
        return NSAccessibilityFrameInView(v, NSMakeRect(r.x, y, r.width, r.height));
    }

    // A screen point to view coordinates with a top-left origin.
    PointF to_view(NSPoint screen) const {
        NSView* v = view;
        if (!v) return {};
        NSPoint p = screen;
        if (v.window) p = [v.window convertPointFromScreen:p];
        p = [v convertPoint:p fromView:nil];
        double y = v.isFlipped ? p.y : NSHeight(v.bounds) - p.y;
        return PointF{p.x, y};
    }
};

}  // namespace broa11y

using broa11y::MacContext;
using broa11y::Node;
using broa11y::NodeId;
using broa11y::Role;
using broa11y::State;

namespace {

NSString* ns(const std::string& s) {
    NSString* out = [NSString stringWithUTF8String:s.c_str()];
    return out ? out : @"";
}
NSString* ns(std::string_view s) { return ns(std::string(s)); }

bool is_text_role(Role r) { return r == Role::TextInput || r == Role::Terminal || r == Role::Document; }

}  // namespace

@implementation BroA11yElement {
    std::shared_ptr<MacContext> _ctx;
    NodeId _node;
}

- (instancetype)initWithContext:(std::shared_ptr<MacContext>)ctx node:(NodeId)node {
    if ((self = [super init])) {
        _ctx = std::move(ctx);
        _node = node;
    }
    return self;
}

- (NodeId)nodeId {
    return _node;
}

- (Node*)node {
    return _ctx && _ctx->tree ? _ctx->tree->get_node(_node) : nullptr;
}

- (BOOL)isAccessibilityElement {
    return [self node] != nullptr;
}

- (NSAccessibilityRole)accessibilityRole {
    Node* n = [self node];
    return n ? ns(broa11y::role_to_mac_role(n->role())) : NSAccessibilityUnknownRole;
}

- (NSAccessibilitySubrole)accessibilitySubrole {
    Node* n = [self node];
    if (!n) return nil;
    std::string_view sub = broa11y::role_to_mac_subrole(n->role());
    return sub.empty() ? nil : ns(sub);
}

- (NSString*)accessibilityRoleDescription {
    return NSAccessibilityRoleDescription([self accessibilityRole], [self accessibilitySubrole]);
}

- (NSString*)accessibilityLabel {
    Node* n = [self node];
    return n && !n->name().empty() ? ns(n->name()) : nil;
}

- (NSString*)accessibilityTitle {
    // Windows carry their name as a title; other elements as their label.
    Node* n = [self node];
    if (!n || (n->role() != Role::Window && n->role() != Role::Dialog)) return nil;
    return ns(n->name());
}

- (NSString*)accessibilityHelp {
    Node* n = [self node];
    return n && !n->description().empty() ? ns(n->description()) : nil;
}

- (NSString*)accessibilityIdentifier {
    return [NSString stringWithFormat:@"%llu", static_cast<unsigned long long>(_node)];
}

- (id)accessibilityValue {
    Node* n = [self node];
    if (!n) return nil;
    if (n->role() == Role::CheckBox || n->role() == Role::RadioButton) {
        return @(n->has_state(State::Indeterminate) ? 2 : (n->has_state(State::Checked) ? 1 : 0));
    }
    if (n->value()) return @(n->value()->current);
    if (is_text_role(n->role()) || !n->text().empty()) return ns(n->text());
    return nil;
}

- (void)setAccessibilityValue:(id)value {
    Node* n = [self node];
    if (!n) return;
    broa11y::ActionParams p;
    if ([value isKindOfClass:[NSString class]]) {
        p.string_val = [static_cast<NSString*>(value) UTF8String];
    } else if ([value isKindOfClass:[NSNumber class]]) {
        p.number_val = [static_cast<NSNumber*>(value) doubleValue];
    } else {
        return;
    }
    n->perform_action(broa11y::kActionSetValue, p);
}

- (id)accessibilityMinValue {
    Node* n = [self node];
    return n && n->value() ? @(n->value()->minimum) : nil;
}

- (id)accessibilityMaxValue {
    Node* n = [self node];
    return n && n->value() ? @(n->value()->maximum) : nil;
}

- (NSRect)accessibilityFrame {
    Node* n = [self node];
    return n ? _ctx->to_screen(n->bounds()) : NSZeroRect;
}

- (id)accessibilityParent {
    Node* n = [self node];
    if (!n) return nil;
    if (_ctx->tree->root_id() == _node) return _ctx->view;
    return _ctx->element(n->parent_id());
}

- (NSArray*)accessibilityChildren {
    Node* n = [self node];
    if (!n) return @[];
    NSMutableArray* out = [NSMutableArray arrayWithCapacity:n->child_count()];
    for (NodeId c : n->children_ids()) {
        if (BroA11yElement* e = _ctx->element(c)) [out addObject:e];
    }
    return out;
}

- (BOOL)isAccessibilityEnabled {
    Node* n = [self node];
    return n && !n->has_state(State::Disabled);
}

- (BOOL)isAccessibilityFocused {
    return [self node] && _ctx->tree->focused_node_id() == _node;
}

- (void)setAccessibilityFocused:(BOOL)focused {
    Node* n = [self node];
    if (n && focused) n->perform_action(broa11y::kActionFocus);
}

- (id)accessibilityHitTest:(NSPoint)point {
    if (![self node]) return nil;
    NodeId hit = _ctx->tree->hit_test_from(_node, _ctx->to_view(point));
    return hit == broa11y::kInvalidNodeId ? nil : _ctx->element(hit);
}

- (id)accessibilityFocusedUIElement {
    if (![self node]) return nil;
    NodeId f = _ctx->tree->focused_node_id();
    return f == broa11y::kInvalidNodeId ? nil : _ctx->element(f);
}

- (BOOL)accessibilityPerformPress {
    Node* n = [self node];
    return n && !n->has_state(State::Disabled) && n->perform_action(broa11y::kActionActivate);
}

- (BOOL)stepValue:(double)sign {
    Node* n = [self node];
    if (!n || !n->value()) return NO;
    const auto& v = *n->value();
    broa11y::ActionParams p;
    p.number_val = std::min(v.maximum, std::max(v.minimum, v.current + sign * v.step));
    return n->perform_action(broa11y::kActionSetValue, p);
}

- (BOOL)accessibilityPerformIncrement {
    return [self stepValue:1.0];
}

- (BOOL)accessibilityPerformDecrement {
    return [self stepValue:-1.0];
}

// ── Text ─────────────────────────────────────────────────────────────────────

- (BOOL)textual {
    Node* n = [self node];
    return n && (is_text_role(n->role()) || !n->text().empty());
}

- (NSInteger)accessibilityNumberOfCharacters {
    return [self textual] ? broa11y::text::TextIndex([self node]->text()).utf16_size() : 0;
}

- (NSRange)accessibilitySelectedTextRange {
    if (![self textual]) return NSMakeRange(NSNotFound, 0);
    Node* n = [self node];
    broa11y::text::TextIndex t(n->text());
    if (!n->selection().is_empty()) {
        NSUInteger s = t.utf16_from_cp(t.cp_from_byte(n->selection().start_offset));
        NSUInteger e = t.utf16_from_cp(t.cp_from_byte(n->selection().end_offset));
        return NSMakeRange(s, e - s);
    }
    if (n->caret_offset() < 0) return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(t.utf16_from_cp(t.cp_from_byte(n->caret_offset())), 0);
}

- (void)setAccessibilitySelectedTextRange:(NSRange)range {
    if (![self textual] || range.location == NSNotFound) return;
    Node* n = [self node];
    broa11y::text::TextIndex t(n->text());
    broa11y::ActionParams p;
    int32_t s = t.cp_from_utf16(static_cast<int32_t>(range.location));
    int32_t e = t.cp_from_utf16(static_cast<int32_t>(range.location + range.length));
    p.range_val = broa11y::TextRange{t.byte_from_cp(s), t.byte_from_cp(e)};
    n->perform_action(broa11y::kActionSetSelection, p);
}

- (NSString*)accessibilitySelectedText {
    NSRange r = [self accessibilitySelectedTextRange];
    if (r.location == NSNotFound) return nil;
    return [self accessibilityStringForRange:r];
}

- (NSRange)accessibilityVisibleCharacterRange {
    return NSMakeRange(0, static_cast<NSUInteger>([self accessibilityNumberOfCharacters]));
}

- (NSString*)accessibilityStringForRange:(NSRange)range {
    if (![self textual]) return nil;
    broa11y::text::TextIndex t([self node]->text());
    int32_t s = t.cp_from_utf16(static_cast<int32_t>(range.location));
    int32_t e = t.cp_from_utf16(static_cast<int32_t>(range.location + range.length));
    return ns(t.slice(s, e));
}

- (NSRange)unitRange:(NSInteger)index unit:(broa11y::TextGranularity)unit {
    broa11y::text::TextIndex t([self node]->text());
    auto [s, e] = broa11y::text::unit_at(t, t.cp_from_utf16(static_cast<int32_t>(index)), unit);
    return NSMakeRange(t.utf16_from_cp(s), t.utf16_from_cp(e) - t.utf16_from_cp(s));
}

- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
    if (![self textual]) return NSMakeRange(NSNotFound, 0);
    return [self unitRange:index unit:broa11y::TextGranularity::Character];
}

- (NSInteger)accessibilityLineForIndex:(NSInteger)index {
    if (![self textual]) return 0;
    broa11y::text::TextIndex t([self node]->text());
    int32_t cp = t.cp_from_utf16(static_cast<int32_t>(index));
    NSInteger line = 0;
    for (int32_t k = 0; k < cp && k < t.size(); ++k) {
        if (t.at(k) == U'\n') ++line;
    }
    return line;
}

- (NSRange)accessibilityRangeForLine:(NSInteger)line {
    if (![self textual] || line < 0) return NSMakeRange(NSNotFound, 0);
    broa11y::text::TextIndex t([self node]->text());
    int32_t k = 0;
    for (NSInteger l = 0; l < line && k < t.size(); ++k) {
        if (t.at(k) == U'\n') ++l;
    }
    if (t.size() == 0) return NSMakeRange(0, 0);
    return [self unitRange:t.utf16_from_cp(k) unit:broa11y::TextGranularity::Line];
}

- (NSInteger)accessibilityInsertionPointLineNumber {
    NSRange r = [self accessibilitySelectedTextRange];
    return r.location == NSNotFound ? 0 : [self accessibilityLineForIndex:static_cast<NSInteger>(r.location)];
}

- (NSRect)accessibilityFrameForRange:(NSRange)range {
    (void)range;  // no glyph geometry in the model: the node's frame
    return [self accessibilityFrame];
}

@end

namespace broa11y {

class MacBridge::Impl {
public:
    explicit Impl(MacBridgeConfig config) : config_(std::move(config)) {}
    ~Impl() { shutdown(); }

    bool initialize(Tree* tree) {
        if (active_) return true;
        error_.clear();
        if (!tree) return fail("no tree");
        NSView* view = (__bridge NSView*)config_.ns_view;
        if (!view) return fail("MacBridgeConfig::ns_view is null; the tree needs a view to live in");
        if (![NSThread isMainThread]) return fail("initialize() must run on the main thread, where AppKit asks");
        ctx_ = std::make_shared<MacContext>();
        ctx_->weak_self = ctx_;
        ctx_->tree = tree;
        ctx_->view = view;
        tree_ = tree;
        attach_root();
        listener_ = tree->add_listener([this](const Event& ev) { handle_event(ev); });
        active_ = true;
        return true;
    }

    void shutdown() {
        if (!active_) return;
        active_ = false;
        if (tree_ && listener_) tree_->remove_listener(listener_);
        listener_ = 0;
        if (NSView* v = ctx_->view) [v setAccessibilityChildren:nil];
        for (auto& [id, e] : ctx_->elements) NSAccessibilityPostNotification(e, NSAccessibilityUIElementDestroyedNotification);
        ctx_->tree = nullptr;
        ctx_->elements.clear();
        ctx_.reset();
        tree_ = nullptr;
    }

    void handle_event(const Event& ev);

    void attach_root() {
        NSView* v = ctx_->view;
        if (!v) return;
        BroA11yElement* root = ctx_->element(tree_->root_id());
        [v setAccessibilityChildren:root ? @[ root ] : @[]];
        attached_root_ = tree_->root_id();
    }

    bool fail(std::string why) {
        error_ = std::move(why);
        return false;
    }

    MacBridgeConfig config_;
    std::shared_ptr<MacContext> ctx_;
    Tree* tree_ = nullptr;
    EventListenerId listener_ = 0;
    NodeId attached_root_ = kInvalidNodeId;
    bool active_ = false;
    std::string error_;
};

void MacBridge::Impl::handle_event(const Event& ev) {
    if (!active_) return;
    if (tree_->root_id() != attached_root_) attach_root();
    auto post = [&](NodeId id, NSAccessibilityNotificationName name) {
        if (BroA11yElement* e = ctx_->element(id)) NSAccessibilityPostNotification(e, name);
    };
    switch (ev.type) {
        case EventType::FocusChanged:
            if (ev.node_id != kInvalidNodeId) post(ev.node_id, NSAccessibilityFocusedUIElementChangedNotification);
            break;
        case EventType::PropertyChanged:
            if (const auto* p = ev.get_if<PropertyChangedPayload>()) {
                if (p->property_name == "name") post(ev.node_id, NSAccessibilityTitleChangedNotification);
                else if (p->property_name == "text") post(ev.node_id, NSAccessibilityValueChangedNotification);
            }
            break;
        case EventType::StateChanged:
            if (const auto* p = ev.get_if<StateChangedPayload>()) {
                if (p->state == State::Checked || p->state == State::Indeterminate) {
                    post(ev.node_id, NSAccessibilityValueChangedNotification);
                } else if (p->state == State::Expanded) {
                    post(ev.node_id, p->enabled ? NSAccessibilityRowExpandedNotification
                                                : NSAccessibilityRowCollapsedNotification);
                }
            }
            break;
        case EventType::ValueChanged:
            post(ev.node_id, NSAccessibilityValueChangedNotification);
            break;
        case EventType::CaretMoved:
        case EventType::TextSelectionChanged:
            post(ev.node_id, NSAccessibilitySelectedTextChangedNotification);
            break;
        case EventType::BoundsChanged:
            post(ev.node_id, NSAccessibilityMovedNotification);
            post(ev.node_id, NSAccessibilityResizedNotification);
            break;
        case EventType::ChildrenChanged:
            if (const auto* p = ev.get_if<ChildrenChangedPayload>()) {
                if (p->change_type == ChildrenChangeType::ChildAdded) post(p->child_id, NSAccessibilityCreatedNotification);
                post(ev.node_id, NSAccessibilityLayoutChangedNotification);
            }
            break;
        case EventType::NodeRemoved: {
            auto it = ctx_->elements.find(ev.node_id);
            if (it != ctx_->elements.end()) {
                NSAccessibilityPostNotification(it->second, NSAccessibilityUIElementDestroyedNotification);
                ctx_->elements.erase(it);
            }
            break;
        }
        case EventType::Announcement:
            if (const auto* p = ev.get_if<AnnouncementPayload>()) {
                id origin = ctx_->element(tree_->contains_node(ev.node_id) ? ev.node_id : tree_->root_id());
                if (!origin) origin = NSApp;
                NSDictionary* info = @{
                    NSAccessibilityAnnouncementKey : ns(p->message),
                    NSAccessibilityPriorityKey : @(p->priority == AnnouncementPriority::Assertive
                                                       ? NSAccessibilityPriorityHigh
                                                       : NSAccessibilityPriorityMedium),
                };
                if (origin) {
                    NSAccessibilityPostNotificationWithUserInfo(origin, NSAccessibilityAnnouncementRequestedNotification,
                                                                info);
                }
            }
            break;
        case EventType::WindowActivated:
            post(ev.node_id, NSAccessibilityMainWindowChangedNotification);
            break;
        case EventType::NodeAdded:
        case EventType::WindowDeactivated:
        case EventType::Count:
            break;
    }
}

MacBridge::MacBridge(MacBridgeConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

MacBridge::~MacBridge() = default;

bool MacBridge::initialize(Tree* tree) { return impl_->initialize(tree); }

void MacBridge::shutdown() { impl_->shutdown(); }

void MacBridge::handle_event(const Event& event) { impl_->handle_event(event); }

void MacBridge::process_events() {}

bool MacBridge::is_active() const noexcept { return impl_->active_; }

std::string MacBridge::last_error() const { return impl_->error_; }

void* MacBridge::element_for(NodeId id) const {
    if (!impl_->active_) return nullptr;
    return (__bridge void*)impl_->ctx_->element(id);
}

void* MacBridge::element_at_screen_point(double x, double y) const {
    if (!impl_->active_) return nullptr;
    NodeId hit = impl_->tree_->hit_test(impl_->ctx_->to_view(NSMakePoint(x, y)));
    return hit == kInvalidNodeId ? nullptr : (__bridge void*)impl_->ctx_->element(hit);
}

}  // namespace broa11y
