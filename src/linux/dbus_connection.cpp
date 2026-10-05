#include "dbus_connection.h"

#if defined(BROA11Y_HAVE_DBUS)
#include <dbus/dbus.h>
#endif

#include <cstdlib>

namespace broa11y::atspi {

struct DbusConnection::RealConnectionData {
#if defined(BROA11Y_HAVE_DBUS)
    DBusConnection* conn = nullptr;
#else
    void* placeholder = nullptr;
#endif
};

DbusConnection::DbusConnection(bool headless_mock)
    : headless_mock_(headless_mock),
      real_conn_(std::make_unique<RealConnectionData>()) {}

DbusConnection::~DbusConnection() {
    disconnect();
}

DbusConnection::DbusConnection(DbusConnection&&) noexcept = default;
DbusConnection& DbusConnection::operator=(DbusConnection&&) noexcept = default;

bool DbusConnection::connect() {
    if (headless_mock_) {
        connected_ = true;
        unique_name_ = ":1.100";
        return true;
    }

#if defined(BROA11Y_HAVE_DBUS)
    const char* a11y_addr = std::getenv("AT_SPI_BUS_ADDRESS");
    DBusError err;
    dbus_error_init(&err);

    if (a11y_addr && a11y_addr[0] != '\0') {
        real_conn_->conn = dbus_connection_open_private(a11y_addr, &err);
        if (real_conn_->conn && !dbus_error_is_set(&err)) {
            dbus_bus_register(real_conn_->conn, &err);
        }
    }

    if (!real_conn_->conn || dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        dbus_error_init(&err);
        // Fallback to session bus
        real_conn_->conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    }

    if (real_conn_->conn && !dbus_error_is_set(&err)) {
        const char* name = dbus_bus_get_unique_name(real_conn_->conn);
        if (name) {
            unique_name_ = name;
        }
        connected_ = true;
        dbus_error_free(&err);
        return true;
    }

    dbus_error_free(&err);
#endif

    // Fallback to in-memory mode if DBus daemon is not running
    connected_ = true;
    unique_name_ = ":1.100";
    return true;
}

void DbusConnection::disconnect() {
#if defined(BROA11Y_HAVE_DBUS)
    if (real_conn_ && real_conn_->conn) {
        dbus_connection_close(real_conn_->conn);
        dbus_connection_unref(real_conn_->conn);
        real_conn_->conn = nullptr;
    }
#endif
    connected_ = false;
}

bool DbusConnection::is_connected() const noexcept {
    return connected_;
}

void DbusConnection::emit_signal(const Signal& signal) {
    emitted_signals_.push_back(signal);

#if defined(BROA11Y_HAVE_DBUS)
    if (real_conn_ && real_conn_->conn) {
        DBusMessage* msg = dbus_message_new_signal(
            signal.path.c_str(),
            signal.interface_name.c_str(),
            signal.member.c_str()
        );
        if (msg) {
            const char* detail_str = signal.detail.c_str();
            int32_t d1 = signal.detail1;
            int32_t d2 = signal.detail2;

            DBusMessageIter iter;
            dbus_message_iter_init_append(msg, &iter);
            dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &detail_str);
            dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &d1);
            dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &d2);

            dbus_connection_send(real_conn_->conn, msg, nullptr);
            dbus_message_unref(msg);
        }
    }
#endif
}

void DbusConnection::flush() {
#if defined(BROA11Y_HAVE_DBUS)
    if (real_conn_ && real_conn_->conn) {
        dbus_connection_flush(real_conn_->conn);
    }
#endif
}

} // namespace broa11y::atspi
