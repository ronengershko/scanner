#include "logging/Logger.h"
#include <chrono>
#include <ctime>
#include <stdexcept>

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

static const char* levelString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
    }
    return "INFO";
}

Logger::Logger(const std::filesystem::path& logPath) {
    m_file.open(logPath, std::ios::app);
    if (!m_file.is_open())
        throw std::runtime_error("Cannot open log file: " + logPath.string());
}

void Logger::log(LogLevel level, const std::string& message) {
    m_file << currentTimestamp() << " " << levelString(level) << " " << message << "\n";
    m_file.flush();
}

void Logger::info(const std::string& message)    { log(LogLevel::Info, message); }
void Logger::warning(const std::string& message) { log(LogLevel::Warning, message); }
void Logger::error(const std::string& message)   { log(LogLevel::Error, message); }
