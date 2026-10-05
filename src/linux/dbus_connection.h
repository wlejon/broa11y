#pragma once

#include "atspi_types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y::atspi {

class DbusConnection {
public:
    DbusConnection();
    ~DbusConnection();

    DbusConnection(const DbusConnection&) = delete;
    DbusConnection& operator=(const DbusConnection&) = delete;
    DbusConnection(DbusConnection&&) noexcept;
    DbusConnection& operator=(DbusConnection&&) noexcept;

    bool connect();
    void disconnect();
    [[nodiscard]] bool is_connected() const noexcept;

    [[nodiscard]] std::string_view unique_name() const noexcept { return unique_name_; }

    void emit_signal(const Signal& signal);
    void flush();

    [[nodiscard]] const std::vector<Signal>& emitted_signals() const noexcept { return emitted_signals_; }
    void clear_emitted_signals() { emitted_signals_.clear(); }

private:
    std::string unique_name_;
    bool connected_ = false;
    std::vector<Signal> emitted_signals_;
    struct RealConnectionData;
    std::unique_ptr<RealConnectionData> real_conn_;
};

} // namespace broa11y::atspi
