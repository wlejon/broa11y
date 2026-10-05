#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace broa11y::atspi {

struct Reference {
    std::string sender;
    std::string path;

    bool operator==(const Reference& other) const = default;
};

struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;

    bool operator==(const Rect& other) const = default;
};

struct Point {
    int32_t x = 0;
    int32_t y = 0;

    bool operator==(const Point& other) const = default;
};

struct TextSlice {
    std::string text;
    int32_t start_offset = 0;
    int32_t end_offset = 0;

    bool operator==(const TextSlice& other) const = default;
};

struct Signal {
    std::string interface_name;
    std::string member;
    std::string path;
    std::string detail;
    int32_t detail1 = 0;
    int32_t detail2 = 0;
    std::string any_data;
};

struct MethodCall {
    std::string path;
    std::string interface_name;
    std::string member;
    std::vector<std::string> args;
};

struct MethodReply {
    bool success = true;
    std::string signature;
    std::vector<std::string> values;
    std::string error_message;
};

} // namespace broa11y::atspi
