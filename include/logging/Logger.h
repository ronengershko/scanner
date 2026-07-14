#pragma once
#include <filesystem>
#include <fstream>
#include <string>

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    explicit Logger(const std::filesystem::path& logPath);

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    std::ofstream m_file;
};
