#include <gtest/gtest.h>
#include "scan/FileScanner.h"
#include "scan/FileTraverser.h"
#include "scan/FileMetadataProvider.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static fs::path makeTempDir() {
    char tmp[] = "/tmp/scanner_scan_test_XXXXXX";
    return fs::path(mkdtemp(tmp));
}

static void writeFile(const fs::path& p, const std::string& s = "hello") {
    std::ofstream f(p); f << s;
}

static Signature makeSig(int64_t id, std::string_view s) {
    return {id, std::vector<std::byte>(
        reinterpret_cast<const std::byte*>(s.data()),
        reinterpret_cast<const std::byte*>(s.data() + s.size()))};
}

// ── FileScanner ───────────────────────────────────────────────────────────────

class ScannerTest : public ::testing::Test {
protected:
    fs::path dir = makeTempDir();
    FileScanner scanner;
    void TearDown() override { fs::remove_all(dir); }
};

TEST_F(ScannerTest, CleanFile) {
    writeFile(dir / "f.txt", "hello world");
    auto r = scanner.scan(dir / "f.txt", {makeSig(1, "evil")});
    EXPECT_EQ(r.verdict, Verdict::Clean);
    EXPECT_FALSE(r.matchedSignatureId.has_value());
}

TEST_F(ScannerTest, MaliciousFile) {
    writeFile(dir / "bad.txt", "contains evil payload");
    auto r = scanner.scan(dir / "bad.txt", {makeSig(7, "evil")});
    EXPECT_EQ(r.verdict, Verdict::Malicious);
    EXPECT_EQ(r.matchedSignatureId, 7);
}

TEST_F(ScannerTest, EmptySignaturesAlwaysClean) {
    writeFile(dir / "f.txt", "evil VIRUS malware");
    auto r = scanner.scan(dir / "f.txt", {});
    EXPECT_EQ(r.verdict, Verdict::Clean);
}

TEST_F(ScannerTest, NonexistentFileIsError) {
    auto r = scanner.scan(dir / "missing.txt", {makeSig(1, "x")});
    EXPECT_EQ(r.verdict, Verdict::Error);
    EXPECT_TRUE(r.errorMessage.has_value());
}

TEST_F(ScannerTest, SignatureAtChunkBoundary) {
    // CHUNK_SIZE = 65536; place "VIRUS" so it spans the boundary
    std::vector<char> data(65536 + 10, 'A');
    const std::string marker = "VIRUS";
    for (size_t i = 0; i < marker.size(); i++) data[65534 + i] = marker[i];

    auto p = dir / "boundary.bin";
    std::ofstream f(p, std::ios::binary);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.close();

    auto r = scanner.scan(p, {makeSig(1, "VIRUS")});
    EXPECT_EQ(r.verdict, Verdict::Malicious);
}

// ── FileTraverser ─────────────────────────────────────────────────────────────

class TraverserTest : public ::testing::Test {
protected:
    fs::path dir = makeTempDir();
    FileTraverser traverser;
    void TearDown() override { fs::remove_all(dir); }

    std::vector<std::string> collect(const fs::path& root) {
        std::vector<std::string> names;
        traverser.traverse(root, [&](const fs::path& p) {
            names.push_back(p.filename().string());
            return true;
        });
        return names;
    }
};

TEST_F(TraverserTest, AlphabeticalOrder) {
    fs::create_directories(dir / "a_dir");
    fs::create_directories(dir / "b_dir");
    writeFile(dir / "a_dir" / "z.txt");
    writeFile(dir / "a_dir" / "a.txt");
    writeFile(dir / "b_dir" / "file.txt");
    writeFile(dir / "m.txt");

    auto names = collect(dir);
    ASSERT_EQ(names.size(), 4u);
    EXPECT_EQ(names[0], "a.txt");
    EXPECT_EQ(names[1], "z.txt");
    EXPECT_EQ(names[2], "file.txt");
    EXPECT_EQ(names[3], "m.txt");
}

TEST_F(TraverserTest, EmptyDirectory) {
    EXPECT_TRUE(collect(dir).empty());
}

TEST_F(TraverserTest, SymlinkSkipped) {
    writeFile(dir / "real.txt");
    fs::create_symlink(dir / "real.txt", dir / "link.txt");
    auto names = collect(dir);
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "real.txt");
}

TEST_F(TraverserTest, CallbackReturnFalseStopsTraversal) {
    writeFile(dir / "a.txt"); writeFile(dir / "b.txt"); writeFile(dir / "c.txt");
    std::vector<std::string> seen;
    traverser.traverse(dir, [&](const fs::path& p) {
        seen.push_back(p.filename().string());
        return seen.size() < 2;
    });
    EXPECT_EQ(seen.size(), 2u);
}

TEST_F(TraverserTest, InaccessibleDirectorySkipped) {
    auto locked = dir / "locked";
    fs::create_directories(locked);
    writeFile(locked / "hidden.txt");
    writeFile(dir / "visible.txt");
    fs::permissions(locked, fs::perms::none);

    auto names = collect(dir);
    fs::permissions(locked, fs::perms::all);

    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "visible.txt");
}

// ── FileMetadataProvider ──────────────────────────────────────────────────────

class MetadataTest : public ::testing::Test {
protected:
    fs::path dir = makeTempDir();
    FileMetadataProvider mp;
    void TearDown() override { fs::remove_all(dir); }
};

TEST_F(MetadataTest, ReadsCorrectly) {
    writeFile(dir / "f.txt", "hello");
    auto m = mp.read(dir / "f.txt");
    EXPECT_EQ(m.size, 5u);
    EXPECT_GT(m.inode, 0u);
    EXPECT_GT(m.deviceId, 0u);
    EXPECT_TRUE(m.normalizedPath.is_absolute());
}

TEST_F(MetadataTest, UnchangedFileMatchesItself) {
    writeFile(dir / "f.txt", "hello");
    auto before = mp.read(dir / "f.txt");
    auto after  = mp.read(dir / "f.txt");
    EXPECT_TRUE(mp.sameFileState(before, after));
}

TEST_F(MetadataTest, SizeChangeDetected) {
    writeFile(dir / "f.txt", "hi");
    auto before = mp.read(dir / "f.txt");
    writeFile(dir / "f.txt", "much longer content now");
    auto after = mp.read(dir / "f.txt");
    EXPECT_FALSE(mp.sameFileState(before, after));
}

TEST_F(MetadataTest, MissingFileThrows) {
    EXPECT_THROW(mp.read(dir / "missing.txt"), std::exception);
}
