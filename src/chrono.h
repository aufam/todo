#pragma once
#include <fmt/chrono.h>
#include <delameta/json.h>

template<> inline std::string
Project::etl::json::serialize(const std::chrono::system_clock::time_point& tp) {
    return fmt::format("\"{:%Y-%m-%d %H:%M:%S}\"", fmt::localtime(
        std::chrono::system_clock::to_time_t(tp))
    );
}

template<> inline size_t
Project::etl::json::size_max(const std::chrono::system_clock::time_point&) {
    constexpr auto res = etl::string_view("\"yyyy-mm-dd HH:MM:SS\"").len();
    return res;
}

template<> inline Project::etl::Result<void, const char*>
Project::etl::json::deserialize(const etl::Json& j, std::chrono::system_clock::time_point& tp) {
    auto sv = j.is_string() ? j.to_string() : j.dump();

    std::tm tm = {};
    std::istringstream ss(std::string(sv.data(), sv.len()));
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        ss = std::istringstream(std::string(sv.data(), sv.len()));
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            return Err("Failed to parse date time");
        }
    }

    tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return Ok();
}
