#include "app/AppConfig.h"
#include <mach-o/dyld.h>
#include <stdexcept>

static std::filesystem::path executableDirectory() {
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        throw std::runtime_error("Failed to resolve executable path");
    return std::filesystem::weakly_canonical(buf).parent_path();
}

AppConfig AppConfig::load() {
    std::filesystem::path projectRoot = executableDirectory().parent_path();
    std::filesystem::path dataRoot = projectRoot / "scanner-data";

    std::filesystem::path dataDir = std::filesystem::weakly_canonical(dataRoot);
    return {
        dataDir,
        dataDir / "scanner.db",
        dataDir / "scanner.log",
        dataDir / "quarantine"
    };
}

void AppConfig::
createRequiredDirectories() const {
    std::error_code ec;
    std::filesystem::create_directories(quarantineDirectory, ec);
    if (ec)
        throw std::runtime_error("Failed to create directories: " + ec.message());
}
