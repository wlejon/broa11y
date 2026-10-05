#include "atspi_serializer.h"

#include <iomanip>
#include <sstream>

namespace broa11y::atspi {

std::string Serializer::encode_string(std::string_view s) {
    std::ostringstream oss;
    oss << "\"" << s << "\"";
    return oss.str();
}

std::string Serializer::encode_int32(int32_t val) {
    return std::to_string(val);
}

std::string Serializer::encode_uint32(uint32_t val) {
    return std::to_string(val);
}

std::string Serializer::encode_double(double val) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << val;
    return oss.str();
}

std::string Serializer::encode_bool(bool val) {
    return val ? "true" : "false";
}

std::string Serializer::encode_reference(const Reference& ref) {
    std::ostringstream oss;
    oss << "(\"" << ref.sender << "\", \"" << ref.path << "\")";
    return oss.str();
}

std::string Serializer::encode_rect(const Rect& rect) {
    std::ostringstream oss;
    oss << "(" << rect.x << ", " << rect.y << ", " << rect.width << ", " << rect.height << ")";
    return oss.str();
}

std::string Serializer::encode_point(const Point& pt) {
    std::ostringstream oss;
    oss << "(" << pt.x << ", " << pt.y << ")";
    return oss.str();
}

std::string Serializer::encode_text_slice(const TextSlice& slice) {
    std::ostringstream oss;
    oss << "(\"" << slice.text << "\", " << slice.start_offset << ", " << slice.end_offset << ")";
    return oss.str();
}

std::string Serializer::encode_state_set(uint32_t low, uint32_t high) {
    std::ostringstream oss;
    oss << "[" << low << ", " << high << "]";
    return oss.str();
}

std::string Serializer::encode_reference_list(const std::vector<Reference>& refs) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < refs.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << encode_reference(refs[i]);
    }
    oss << "]";
    return oss.str();
}

std::string Serializer::encode_string_map(const std::unordered_map<std::string, std::string>& map) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [k, v] : map) {
        if (!first) oss << ", ";
        oss << "\"" << k << "\": \"" << v << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string Serializer::serialize_signal(const Signal& signal) {
    std::ostringstream oss;
    oss << "SIGNAL " << signal.interface_name << "." << signal.member
        << " path=" << signal.path
        << " detail=\"" << signal.detail << "\""
        << " detail1=" << signal.detail1
        << " detail2=" << signal.detail2;
    if (!signal.any_data.empty()) {
        oss << " any_data=" << signal.any_data;
    }
    return oss.str();
}

std::string Serializer::serialize_reply(const MethodReply& reply) {
    if (!reply.success) {
        return "ERROR: " + reply.error_message;
    }
    std::ostringstream oss;
    oss << "REPLY (" << reply.signature << ") [";
    for (size_t i = 0; i < reply.values.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << reply.values[i];
    }
    oss << "]";
    return oss.str();
}

} // namespace broa11y::atspi
