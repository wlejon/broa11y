#pragma once

#include "atspi_types.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace broa11y::atspi {

class Serializer {
public:
    static std::string encode_string(std::string_view s);
    static std::string encode_int32(int32_t val);
    static std::string encode_uint32(uint32_t val);
    static std::string encode_double(double val);
    static std::string encode_bool(bool val);

    static std::string encode_reference(const Reference& ref);
    static std::string encode_rect(const Rect& rect);
    static std::string encode_point(const Point& pt);
    static std::string encode_text_slice(const TextSlice& slice);

    static std::string encode_state_set(uint32_t low, uint32_t high);
    static std::string encode_reference_list(const std::vector<Reference>& refs);
    static std::string encode_string_map(const std::unordered_map<std::string, std::string>& map);

    static std::string serialize_signal(const Signal& signal);
    static std::string serialize_reply(const MethodReply& reply);
};

} // namespace broa11y::atspi
