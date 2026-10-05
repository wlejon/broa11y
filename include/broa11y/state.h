#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y {

enum class State : uint32_t {
    Focused = 0,
    Focusable,
    Selected,
    Selectable,
    Expanded,
    Collapsed,
    Disabled,
    ReadOnly,
    Checked,
    Busy,
    Modal,
    MultiSelectable,
    Visible,
    Showing,
    Sensitive,
    Defunct,
    Active,
    Armed,
    Indeterminate,
    Vertical,
    Horizontal,
    Required,
    Invalid,
    MultiLine,
    SingleLine,
    HasPopup,
    SelectableText,
    Editable,
    Animated,
    Transient,
    Count
};

std::string_view state_to_string(State state);
State string_to_state(std::string_view str);

class StateSet {
public:
    StateSet() noexcept = default;

    explicit StateSet(uint64_t bits) noexcept : bits_(bits) {}

    StateSet& set(State s, bool value = true) noexcept {
        auto idx = static_cast<size_t>(s);
        if (idx < bits_.size()) {
            bits_.set(idx, value);
        }
        return *this;
    }

    StateSet& reset(State s) noexcept {
        return set(s, false);
    }

    [[nodiscard]] bool has(State s) const noexcept {
        auto idx = static_cast<size_t>(s);
        return idx < bits_.size() && bits_.test(idx);
    }

    [[nodiscard]] bool test(State s) const noexcept {
        return has(s);
    }

    void clear() noexcept {
        bits_.reset();
    }

    [[nodiscard]] uint64_t to_uint64() const noexcept {
        return bits_.to_ullong();
    }

    [[nodiscard]] bool empty() const noexcept {
        return bits_.none();
    }

    [[nodiscard]] size_t count() const noexcept {
        return bits_.count();
    }

    bool operator==(const StateSet& other) const noexcept = default;

    StateSet operator|(const StateSet& other) const noexcept {
        StateSet res;
        res.bits_ = bits_ | other.bits_;
        return res;
    }

    StateSet operator&(const StateSet& other) const noexcept {
        StateSet res;
        res.bits_ = bits_ & other.bits_;
        return res;
    }

    StateSet operator^(const StateSet& other) const noexcept {
        StateSet res;
        res.bits_ = bits_ ^ other.bits_;
        return res;
    }

    [[nodiscard]] std::vector<State> to_vector() const;
    [[nodiscard]] std::string to_string() const;

    // AT-SPI StateSet is 2 x 32-bit unsigned integers
    [[nodiscard]] std::pair<uint32_t, uint32_t> to_atspi_state_bitmask() const;

private:
    std::bitset<64> bits_{0};
};

} // namespace broa11y
