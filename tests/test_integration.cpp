#include <gtest/gtest.h>
#include "database/Database.h"
#include "database/CacheRepository.h"
#include "database/ExclusionRepository.h"
#include "database/QuarantineRepository.h"
#include "database/ScanSessionRepository.h"
#include "database/SignatureRepository.h"
#include "exclusions/ExclusionService.h"
#include "logging/Logger.h"
#include "quarantine/QuarantineService.h"
#include "scan/FileMetadataProvider.h"
#include "scan/FileScanner.h"
#include "scan/FileTraverser.h"
#include "scan/ScanService.h"
#include "signatures/SignatureService.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ── TestEnv: full scanner stack backed by a temp directory ────────────────────

struct TestEnv {
    fs::path dir;
    Database db;
    Logger logger;
    SignatureRepository sigRepo;
    ScanSessionRepository sessionRepo;
    CacheRepository cacheRepo;
    QuarantineRepository quarantineRepo;
    ExclusionRepository exclusionRepo;
    SignatureService sigService;
    QuarantineService quarantineService;
    ExclusionService exclusionService;
    FileTraverser traverser;
    FileScanner fileScanner;
    FileMetadataProvider metaProvider;
    ScanService scanService;

    static fs::path makeDir() {
        char tmp[] = "/tmp/scanner_int_test_XXXXXX";
        fs::path d = mkdtemp(tmp);
        fs::create_directories(d / "quarantine");
        return d;
    }

    TestEnv()
        : dir(makeDir())
        , db(dir / "scanner.db")
        , logger(dir / "scanner.log")
        , sigRepo(db), sessionRepo(db), cacheRepo(db)
        , quarantineRepo(db), exclusionRepo(db)
        , sigService(sigRepo, logger)
        , quarantineService(quarantineRepo, logger, dir / "quarantine")
        , exclusionService(exclusionRepo, logger)
        , scanService(traverser, fileScanner, metaProvider, sessionRepo,
                      sigService, cacheRepo, quarantineService, exclusionService, logger)
    {
        db.initializeSchema();
    }

    ~TestEnv() { fs::remove_all(dir); }

    fs::path scanDir() {
        auto d = dir / "scan";
        fs::create_directories(d);
        return d;
    }

    void write(const fs::path& p, const std::string& content) {
        std::ofstream(p) << content;
    }

    // Read session counters directly from the DB
    struct Counters { int64_t scanned, cacheHits, malicious, excluded, errors; };
    Counters getCounters(int64_t sessionId) {
        Stmt stmt;
        sqlite3_prepare_v2(db.handle(),
            "SELECT scanned_files, cache_hits, malicious_files, excluded_files, error_files"
            " FROM scan_sessions WHERE id = ?",
            -1, &stmt.ptr, nullptr);
        sqlite3_bind_int64(stmt.ptr, 1, sessionId);
        sqlite3_step(stmt.ptr);
        return {sqlite3_column_int64(stmt.ptr, 0), sqlite3_column_int64(stmt.ptr, 1),
                sqlite3_column_int64(stmt.ptr, 2), sqlite3_column_int64(stmt.ptr, 3),
                sqlite3_column_int64(stmt.ptr, 4)};
    }

    int64_t latestSessionId() {
        Stmt stmt;
        sqlite3_prepare_v2(db.handle(),
            "SELECT id FROM scan_sessions ORDER BY id DESC LIMIT 1",
            -1, &stmt.ptr, nullptr);
        if (sqlite3_step(stmt.ptr) != SQLITE_ROW) return -1;
        return sqlite3_column_int64(stmt.ptr, 0);
    }
};

class IntegrationTest : public ::testing::Test {
protected:
    TestEnv env;
    // Suppress stdout from scan summaries so test output is clean
    std::streambuf* oldCout = nullptr;
    void SetUp() override { oldCout = std::cout.rdbuf(nullptr); }
    void TearDown() override { std::cout.rdbuf(oldCout); }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(IntegrationTest, ScanCleanFile) {
    auto d = env.scanDir();
    env.write(d / "clean.txt", "nothing bad here");

    EXPECT_EQ(env.scanService.scanPath(d), 0);
    auto c = env.getCounters(env.latestSessionId());
    EXPECT_EQ(c.scanned, 1);
    EXPECT_EQ(c.malicious, 0);
    EXPECT_TRUE(env.cacheRepo.lookup(fs::weakly_canonical(d / "clean.txt").string()).has_value());
}

TEST_F(IntegrationTest, ScanMaliciousFileQuarantinesIt) {
    env.sigService.add("VIRUS");
    auto d = env.scanDir();
    env.write(d / "bad.txt", "VIRUS is here");

    EXPECT_EQ(env.scanService.scanPath(d), 0);
    auto c = env.getCounters(env.latestSessionId());
    EXPECT_EQ(c.malicious, 1);
    EXPECT_EQ(env.quarantineRepo.loadAll().size(), 1u);
    EXPECT_FALSE(fs::exists(d / "bad.txt"));  // moved to quarantine
}

TEST_F(IntegrationTest, ScanDirectory) {
    auto d = env.scanDir();
    env.write(d / "a.txt", "aaa");
    env.write(d / "b.txt", "bbb");
    env.write(d / "c.txt", "ccc");

    EXPECT_EQ(env.scanService.scanPath(d), 0);
    auto c = env.getCounters(env.latestSessionId());
    EXPECT_EQ(c.scanned, 3);
    EXPECT_EQ(c.errors, 0);
}

TEST_F(IntegrationTest, SecondScanUsesCacheHits) {
    auto d = env.scanDir();
    env.write(d / "a.txt", "hello");

    env.scanService.scanPath(d);
    EXPECT_EQ(env.getCounters(env.latestSessionId()).cacheHits, 0);

    env.scanService.scanPath(d);
    EXPECT_EQ(env.getCounters(env.latestSessionId()).cacheHits, 1);
}

TEST_F(IntegrationTest, ExcludedDirectorySkipped) {
    auto d = env.scanDir();
    auto privateDir = d / "private";
    fs::create_directories(privateDir);
    env.write(d / "public.txt", "public");
    env.write(privateDir / "secret.txt", "secret");

    env.exclusionService.add(privateDir);

    EXPECT_EQ(env.scanService.scanPath(d), 0);
    auto c = env.getCounters(env.latestSessionId());
    EXPECT_EQ(c.scanned, 1);   // only public.txt
    EXPECT_EQ(c.excluded, 1);  // secret.txt excluded
}

TEST_F(IntegrationTest, StopWithNoRunningSession) {
    EXPECT_EQ(env.scanService.requestStop(), 1);
}

TEST_F(IntegrationTest, ResumeFromCheckpoint) {
    auto d = env.scanDir();
    env.write(d / "a.txt", "aaa");
    env.write(d / "b.txt", "bbb");
    env.write(d / "c.txt", "ccc");

    // Manually create a stopped session with checkpoint at a.txt
    std::string canonical = fs::weakly_canonical(d).string();
    auto id = env.sessionRepo.create(canonical, canonical, "path");
    env.sessionRepo.updateStatus(id, "stopped");
    env.sessionRepo.updateCheckpoint(id, (fs::weakly_canonical(d) / "a.txt").string());

    EXPECT_EQ(env.scanService.resume(), 0);
    // Only b.txt and c.txt should be processed (a.txt was already done)
    EXPECT_EQ(env.getCounters(id).scanned, 2);
}
