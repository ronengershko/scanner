#include <gtest/gtest.h>
#include "database/Database.h"
#include "database/CacheRepository.h"
#include "database/SignatureRepository.h"
#include "database/ExclusionRepository.h"
#include "scan/FileMetadataProvider.h"
#include <filesystem>

namespace fs = std::filesystem;

struct DbEnv {
    fs::path dir;
    Database db;

    static fs::path makeDir() {
        char tmp[] = "/tmp/scanner_db_test_XXXXXX";
        return fs::path(mkdtemp(tmp));
    }

    DbEnv() : dir(makeDir()), db(dir / "test.db") { db.initializeSchema(); }
    ~DbEnv() { fs::remove_all(dir); }
};

static FileMetadata fakeMeta(uint64_t dev, uint64_t ino, uintmax_t size, int64_t mtime) {
    return {fs::path("/tmp/f.txt"), dev, ino, size, mtime};
}

// ── CacheRepository::isValidHit (pure logic, no DB) ───────────────────────────

TEST(CacheHit, AllFieldsMatch) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_TRUE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 100, 999), 5));
}

TEST(CacheHit, MissOnSizeChange) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 200, 999), 5));
}

TEST(CacheHit, MissOnMtimeChange) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 100, 1000), 5));
}

TEST(CacheHit, MissOnInodeChange) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(1, 99, 100, 999), 5));
}

TEST(CacheHit, MissOnDeviceChange) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(99, 2, 100, 999), 5));
}

TEST(CacheHit, MissOnSigVersionChange) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "clean"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 100, 999), 6));
}

TEST(CacheHit, MissOnErrorVerdict) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "error"};
    EXPECT_FALSE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 100, 999), 5));
}

TEST(CacheHit, MaliciousVerdictIsHit) {
    CacheRecord r{"p", 1, 2, 100, 999, 5, "malicious"};
    EXPECT_TRUE(CacheRepository::isValidHit(r, fakeMeta(1, 2, 100, 999), 5));
}

// ── CacheRepository persistence ───────────────────────────────────────────────

TEST(CacheDb, LookupMissOnEmpty) {
    DbEnv e;
    CacheRepository repo(e.db);
    EXPECT_FALSE(repo.lookup("/nonexistent").has_value());
}

TEST(CacheDb, UpsertAndLookup) {
    DbEnv e;
    CacheRepository repo(e.db);
    repo.upsert({"/f.txt", 1, 2, 100, 999, 5, "clean"});
    auto r = repo.lookup("/f.txt");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->verdict, "clean");
    EXPECT_EQ(r->signatureVersion, 5);
    EXPECT_EQ(r->deviceId, 1);
}

TEST(CacheDb, UpsertUpdatesExisting) {
    DbEnv e;
    CacheRepository repo(e.db);
    repo.upsert({"/f.txt", 1, 2, 100, 999, 5, "clean"});
    repo.upsert({"/f.txt", 1, 2, 200, 1001, 6, "malicious"});
    auto r = repo.lookup("/f.txt");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->size, 200);
    EXPECT_EQ(r->verdict, "malicious");
    EXPECT_EQ(r->signatureVersion, 6);
}

TEST(CacheDb, PersistsAcrossRestart) {
    DbEnv e;
    {
        Database db(e.dir / "test.db");
        db.initializeSchema();
        CacheRepository repo(db);
        repo.upsert({"/f.txt", 3, 7, 42, 500, 2, "clean"});
    }  // db closed here
    Database db2(e.dir / "test.db");
    db2.initializeSchema();
    CacheRepository repo2(db2);
    auto r = repo2.lookup("/f.txt");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->inode, 7);
    EXPECT_EQ(r->verdict, "clean");
}

// ── SignatureRepository ───────────────────────────────────────────────────────

TEST(Signatures, AddAndLoad) {
    DbEnv e;
    SignatureRepository repo(e.db);
    repo.add("deadbeef");
    auto sigs = repo.loadAll();
    ASSERT_EQ(sigs.size(), 1u);
    EXPECT_EQ(sigs[0].value, "deadbeef");
}

TEST(Signatures, DuplicateThrows) {
    DbEnv e;
    SignatureRepository repo(e.db);
    repo.add("deadbeef");
    EXPECT_THROW(repo.add("deadbeef"), std::runtime_error);
}

TEST(Signatures, RemoveNotFoundThrows) {
    DbEnv e;
    SignatureRepository repo(e.db);
    EXPECT_THROW(repo.remove(999), std::runtime_error);
}

TEST(Signatures, VersionIncrementsOnAddAndRemove) {
    DbEnv e;
    SignatureRepository repo(e.db);
    int64_t v0 = repo.getVersion();
    repo.add("aabbcc");
    EXPECT_EQ(repo.getVersion(), v0 + 1);
    int64_t id = repo.loadAll()[0].id;
    repo.remove(id);
    EXPECT_EQ(repo.getVersion(), v0 + 2);
}

TEST(Signatures, LoadAll) {
    DbEnv e;
    SignatureRepository repo(e.db);
    repo.add("aaa");
    repo.add("bbb");
    EXPECT_EQ(repo.loadAll().size(), 2u);
}

// ── ExclusionRepository ───────────────────────────────────────────────────────

TEST(Exclusions, AddDirectory) {
    DbEnv e;
    ExclusionRepository repo(e.db);
    repo.add("/tmp/safe", true);
    auto ex = repo.loadAll();
    ASSERT_EQ(ex.size(), 1u);
    EXPECT_EQ(ex[0].path, "/tmp/safe");
    EXPECT_TRUE(ex[0].isDirectory);
}

TEST(Exclusions, AddFile) {
    DbEnv e;
    ExclusionRepository repo(e.db);
    repo.add("/tmp/safe.txt", false);
    auto ex = repo.loadAll();
    ASSERT_EQ(ex.size(), 1u);
    EXPECT_FALSE(ex[0].isDirectory);
}

TEST(Exclusions, Remove) {
    DbEnv e;
    ExclusionRepository repo(e.db);
    int64_t id = repo.add("/tmp/safe", true);
    repo.remove(id);
    EXPECT_TRUE(repo.loadAll().empty());
}

TEST(Exclusions, RemoveNotFoundThrows) {
    DbEnv e;
    ExclusionRepository repo(e.db);
    EXPECT_THROW(repo.remove(999), std::runtime_error);
}
