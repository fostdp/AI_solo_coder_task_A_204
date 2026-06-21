#include "common.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace stone_mill {

std::string to_string(AlertType type) {
    switch (type) {
        case AlertType::WEAR: return "wear";
        case AlertType::LOW_YIELD: return "low_yield";
        case AlertType::ABNORMAL_SPEED: return "abnormal_speed";
        case AlertType::ABNORMAL_PRESSURE: return "abnormal_pressure";
    }
    return "unknown";
}

std::string to_string(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::INFO: return "info";
        case AlertSeverity::WARNING: return "warning";
        case AlertSeverity::CRITICAL: return "critical";
    }
    return "unknown";
}

AlertType alert_type_from_string(const std::string& s) {
    if (s == "wear") return AlertType::WEAR;
    if (s == "low_yield") return AlertType::LOW_YIELD;
    if (s == "abnormal_speed") return AlertType::ABNORMAL_SPEED;
    if (s == "abnormal_pressure") return AlertType::ABNORMAL_PRESSURE;
    throw std::invalid_argument("Unknown alert type: " + s);
}

AlertSeverity alert_severity_from_string(const std::string& s) {
    if (s == "info") return AlertSeverity::INFO;
    if (s == "warning") return AlertSeverity::WARNING;
    if (s == "critical") return AlertSeverity::CRITICAL;
    throw std::invalid_argument("Unknown alert severity: " + s);
}

}
