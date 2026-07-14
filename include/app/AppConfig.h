#pragma once
#include <filesystem>

struct AppConfig {
    std::filesystem::path databasePath;
    std::filesystem::path logPath;
    std::filesystem::path quarantineDirectory;
    std::filesystem::path fullScanRoot;

    static AppConfig load();
    void createRequiredDirectories() const;
};
