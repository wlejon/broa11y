#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

using NodeId = uint64_t;
constexpr NodeId kInvalidNodeId = 0;
constexpr NodeId kRootNodeId = 1;

struct PointF {
    double x = 0.0;
    double y = 0.0;

    constexpr bool operator==(const PointF& other) const = default;
};

struct SizeF {
    double width = 0.0;
    double height = 0.0;

    constexpr bool operator==(const SizeF& other) const = default;
};

struct RectF {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    constexpr bool operator==(const RectF& other) const = default;

    constexpr bool contains(PointF pt) const noexcept {
        return pt.x >= x && pt.x <= (x + width) &&
               pt.y >= y && pt.y <= (y + height);
    }

    constexpr bool intersects(const RectF& other) const noexcept {
        return x < (other.x + other.width) && (x + width) > other.x &&
               y < (other.y + other.height) && (y + height) > other.y;
    }

    constexpr bool is_empty() const noexcept {
        return width <= 0.0 || height <= 0.0;
    }
};

struct TextRange {
    int32_t start_offset = 0;
    int32_t end_offset = 0;

    constexpr bool operator==(const TextRange& other) const = default;

    constexpr bool is_empty() const noexcept {
        return start_offset >= end_offset;
    }

    constexpr int32_t length() const noexcept {
        return end_offset > start_offset ? (end_offset - start_offset) : 0;
    }

    constexpr bool contains_offset(int32_t offset) const noexcept {
        return offset >= start_offset && offset < end_offset;
    }
};

struct ValueRange {
    double current = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    double step = 1.0;

    constexpr bool operator==(const ValueRange& other) const = default;

    constexpr double clamped_value() const noexcept {
        if (current < minimum) return minimum;
        if (current > maximum) return maximum;
        return current;
    }
};

enum class TextGranularity : uint8_t {
    Character = 0,
    Word,
    Line,
    Paragraph,
    Document
};

enum class AnnouncementPriority : uint8_t {
    Polite = 0,
    Assertive = 1
};

enum class RelationType : uint32_t {
    ControlledBy = 0,
    ControllerFor,
    DescribedBy,
    DescriptionFor,
    LabelledBy,
    LabelFor,
    MemberOf,
    NodeChildOf,
    FlowsTo,
    FlowsFrom,
    SubwindowOf
};

std::string_view relation_type_to_string(RelationType relation);

} // namespace broa11y
