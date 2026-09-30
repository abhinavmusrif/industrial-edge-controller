#include "telemetry.hpp"
#include <sstream>
#include <iomanip>

namespace industrial {

std::string SystemTelemetry::to_json() const {
    std::ostringstream oss;
    oss << "{\n"
        << "  \"device\": \"" << device_id << "\",\n"
        << "  \"timestamp\": \"" << iso_timestamp << "\",\n"
        << "  \"epoch_ms\": " << epoch_ms << ",\n"
        << "  \"temperature\": " << std::fixed << std::setprecision(1) << temperature << ",\n"
        << "  \"current\": " << std::fixed << std::setprecision(2) << current << ",\n"
        << "  \"vibration\": " << std::fixed << std::setprecision(2) << vibration << ",\n"
        << "  \"rpm\": " << std::fixed << std::setprecision(0) << rpm << ",\n"
        << "  \"state\": \"" << controller_state << "\",\n"
        << "  \"mcu_connected\": " << (mcu_connected ? "true" : "false") << ",\n"
        << "  \"watchdog\": \"" << (watchdog_ok ? "OK" : "EXPIRED") << "\",\n"
        << "  \"watchdog_elapsed_ms\": " << watchdog_elapsed_ms << ",\n"
        << "  \"last_trip_reason\": \"" << last_trip_reason << "\",\n"
        << "  \"faults\": [";

    for (size_t i = 0; i < active_faults.size(); ++i) {
        oss << "\"" << active_faults[i] << "\"";
        if (i + 1 < active_faults.size()) oss << ", ";
    }
    oss << "]\n}";
    return oss.str();
}

} // namespace industrial
