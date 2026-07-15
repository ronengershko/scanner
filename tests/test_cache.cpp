#include "database/CacheRepository.h"
#include "database/Database.h"
#include "scan/FileMetadataProvider.h"
#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static int passed = 0;
static int failed = 0;

#define CHECK(condition, name) \
    do { \
        if (condition) { std::cout << "[PASS] " << (name) << "\n"; passed++; } \
        else           { std::cout << "[FAIL] " << (name) << "\n"; failed++; } \
    } while (0)

static fs::path tempDbPath() {
    return fs::temp_directory_path() / "scanner_cache_test.db";
}

static FileMetadata makeMetadata(uint64_t dev, uint64_t ino,
                                  uintmax_t size, int64_t mtime) {
    return {fs::path("/tmp/test.txt"), dev, ino, size, mtime};
}

// ── isValidHit logic ─────────────────────────────────────────────────────────

static void test_hit_when_all_match() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(1, 2, 100, 999);
    CHECK(CacheRepository::isValidHit(r, m, 5), "hit: all fields match");
}

static void test_miss_on_size_change() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(1, 2, 200, 999); // size changed
    CHECK(!CacheRepository::isValidHit(r, m, 5), "miss: size changed");
}

static void test_miss_on_mtime_change() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(1, 2, 100, 1000); // mtime changed
    CHECK(!CacheRepository::isValidHit(r, m, 5), "miss: mtime changed");
}

static void test_miss_on_inode_change() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(1, 99, 100, 999); // inode changed
    CHECK(!CacheRepository::isValidHit(r, m, 5), "miss: inode changed");
}

static void test_miss_on_device_change() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(99, 2, 100, 999); // device changed
    CHECK(!CacheRepository::isValidHit(r, m, 5), "miss: device changed");
}

static void test_miss_on_signature_version_change() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "clean"};
    FileMetadata m = makeMetadata(1, 2, 100, 999);
    CHECK(!CacheRepository::isValidHit(r, m, 6), "miss: signature version changed");
}

static void test_miss_on_error_verdict() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "error"};
    FileMetadata m = makeMetadata(1, 2, 100, 999);
    CHECK(!CacheRepository::isValidHit(r, m, 5), "miss: error verdict not reused");
}

static void test_malicious_verdict_is_hit() {
    CacheRecord r{"path", 1, 2, 100, 999, 5, "malicious"};
    FileMetadata m = makeMetadata(1, 2, 100, 999);
    CHECK(CacheRepository::isValidHit(r, m, 5), "hit: malicious verdict reused");
}

// ── Persistence via Database ──────────────────────────────────────────────────

static void test_lookup_miss_on_empty_db() {
    fs::remove(tempDbPath());
    Database db(tempDbPath());
    db.initializeSchema();
    CacheRepository repo(db);

    CHECK(!repo.lookup("/nonexistent").has_value(), "persistence: lookup miss on empty db");
}

static void test_upsert_and_lookup() {
    fs::remove(tempDbPath());
    Database db(tempDbPath());
    db.initializeSchema();
    CacheRepository repo(db);

    CacheRecord r{"/tmp/test.txt", 1, 2, 100, 999, 5, "clean"};
    repo.upsert(r);

    auto found = repo.lookup("/tmp/test.txt");
    CHECK(found.has_value(), "persistence: upsert then lookup finds record");
    CHECK(found->verdict == "clean", "persistence: verdict matches");
    CHECK(found->deviceId == 1, "persistence: deviceId matches");
    CHECK(found->signatureVersion == 5, "persistence: signature version matches");
}

static void test_upsert_updates_existing() {
    fs::remove(tempDbPath());
    Database db(tempDbPath());
    db.initializeSchema();
    CacheRepository repo(db);

    repo.upsert({"/tmp/test.txt", 1, 2, 100, 999, 5, "clean"});
    repo.upsert({"/tmp/test.txt", 1, 2, 200, 1001, 6, "malicious"}); // updated

    auto found = repo.lookup("/tmp/test.txt");
    CHECK(found.has_value(), "persistence: record exists after update");
    CHECK(found->size == 200, "persistence: size updated");
    CHECK(found->verdict == "malicious", "persistence: verdict updated");
    CHECK(found->signatureVersion == 6, "persistence: signature version updated");
}

static void test_persists_across_restart() {
    fs::remove(tempDbPath());
    {
        Database db(tempDbPath());
        db.initializeSchema();
        CacheRepository repo(db);
        repo.upsert({"/tmp/f.txt", 3, 7, 42, 500, 2, "clean"});
    }
    // Re-open
    Database db2(tempDbPath());
    db2.initializeSchema();
    CacheRepository repo2(db2);
    auto found = repo2.lookup("/tmp/f.txt");
    CHECK(found.has_value(), "persistence: record survives restart");
    CHECK(found->inode == 7, "persistence: inode correct after restart");
}

// ────────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== CacheRepository::isValidHit tests ===\n";
    test_hit_when_all_match();
    test_miss_on_size_change();
    test_miss_on_mtime_change();
    test_miss_on_inode_change();
    test_miss_on_device_change();
    test_miss_on_signature_version_change();
    test_miss_on_error_verdict();
    test_malicious_verdict_is_hit();

    std::cout << "\n=== CacheRepository persistence tests ===\n";
    test_lookup_miss_on_empty_db();
    test_upsert_and_lookup();
    test_upsert_updates_existing();
    test_persists_across_restart();

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed > 0 ? 1 : 0;
}
