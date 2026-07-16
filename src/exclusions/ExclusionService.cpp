#include "exclusions/ExclusionService.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

// True if base is a prefix of candidate using path-component comparison.
// /a/b is a subpath of /a/b/c but NOT of /a/bc.
static bool isSubpath(const fs::path& base, const fs::path& candidate) {
    auto b = base.begin(), be = base.end();
    auto c = candidate.begin(), ce = candidate.end();
    for (; b != be; ++b, ++c) {
        if (c == ce || *b != *c) return false;
    }
    return true;
}

ExclusionService::ExclusionService(ExclusionRepository& repo, Logger& logger)
    : m_repo(repo), m_logger(logger) {}

bool ExclusionService::isExcluded(const fs::path& path) const {
    fs::path p = fs::weakly_canonical(path);
    for (const auto& e : m_repo.loadAll()) {
        fs::path ep(e.path);
        if (e.isDirectory ? isSubpath(ep, p) : p == ep)
            return true;
    }
    return false;
}

void ExclusionService::list() const {
    auto excls = m_repo.loadAll();
    if (excls.empty()) {
        std::cout << "No exclusions.\n";
        return;
    }
    std::cout << "ID    Type       Path\n";
    for (const auto& e : excls)
        std::cout << std::left << std::setw(6) << e.id
                  << std::setw(11) << (e.isDirectory ? "directory" : "file")
                  << e.path << "\n";
}

void ExclusionService::add(const fs::path& path) {
    fs::path p = fs::weakly_canonical(path);
    std::string pathStr = p.string();

    for (const auto& e : m_repo.loadAll()) {
        if (e.path == pathStr)
            throw std::runtime_error("Exclusion already exists: " + pathStr);
    }

    bool isDir = fs::exists(p) && fs::is_directory(p);
    m_repo.add(pathStr, isDir);
    m_logger.info("Exclusion added: " + pathStr);
    std::cout << "Excluded " << (isDir ? "directory" : "file") << ": " << pathStr << "\n";
}

void ExclusionService::remove(int64_t id) {
    m_repo.remove(id);
    m_logger.info("Exclusion removed id=" + std::to_string(id));
    std::cout << "Exclusion removed.\n";
}
