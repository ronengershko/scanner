#include "database/RepositoryUtils.h"
#include <chrono>
#include <ctime>
#include <stdexcept>

std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

void check(int rc, sqlite3* db) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(db));
}
