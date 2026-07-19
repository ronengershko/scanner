#pragma once
#include <filesystem>

struct AppConfig {
    std::filesystem::path dataDirectory;
    std::filesystem::path databasePath;
    std::filesystem::path logPath;
    std::filesystem::path quarantineDirectory;

    static AppConfig load();
    void createRequiredDirectories() const;
};
