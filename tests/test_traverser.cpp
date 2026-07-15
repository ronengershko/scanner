#include "scan/FileMetadataProvider.h"
#include "scan/FileTraverser.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static int passed = 0;
static int failed = 0;

#define CHECK(condition, name) \
    do { \
        if (condition) { std::cout << "[PASS] " << (name) << "\n"; passed++; } \
        else           { std::cout << "[FAIL] " << (name) << "\n"; failed++; } \
    } while (0)

static void writeFile(const fs::path& p, const std::string& content = "hello") {
    std::ofstream f(p);
    f << content;
}

static fs::path setupTempDir() {
    fs::path tmp = fs::temp_directory_path() / "scanner_traverser_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    return tmp;
}

// ── FileTraverser tests ──────────────────────────────────────────────────────

static void test_single_file() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "only.txt");

    FileTraverser t;
    std::vector<fs::path> visited;
    t.traverse(tmp / "only.txt", [&](const fs::path& p) {
        visited.push_back(p);
        return true;
    });

    CHECK(visited.size() == 1, "single file: visited exactly once");
    CHECK(visited[0].filename() == "only.txt", "single file: correct name");
}

static void test_alphabetical_order() {
    fs::path tmp = setupTempDir();
    fs::create_directories(tmp / "b_dir");
    fs::create_directories(tmp / "a_dir");
    writeFile(tmp / "b_dir" / "file.txt");
    writeFile(tmp / "a_dir" / "z.txt");
    writeFile(tmp / "a_dir" / "a.txt");
    writeFile(tmp / "m.txt");

    FileTraverser t;
    std::vector<std::string> names;
    t.traverse(tmp, [&](const fs::path& p) {
        names.push_back(p.filename().string());
        return true;
    });

    CHECK(names.size() == 4, "alphabetical: all 4 files visited");
    CHECK(names[0] == "a.txt", "alphabetical: a_dir/a.txt first");
    CHECK(names[1] == "z.txt", "alphabetical: a_dir/z.txt second");
    CHECK(names[2] == "file.txt", "alphabetical: b_dir/file.txt third");
    CHECK(names[3] == "m.txt", "alphabetical: m.txt last");
}

static void test_empty_directory() {
    fs::path tmp = setupTempDir();
    fs::create_directories(tmp / "empty");

    FileTraverser t;
    std::vector<fs::path> visited;
    t.traverse(tmp / "empty", [&](const fs::path& p) {
        visited.push_back(p);
        return true;
    });

    CHECK(visited.empty(), "empty directory: nothing visited");
}

static void test_symlink_skipped() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "real.txt");
    fs::create_symlink(tmp / "real.txt", tmp / "link.txt");

    FileTraverser t;
    std::vector<std::string> names;
    t.traverse(tmp, [&](const fs::path& p) {
        names.push_back(p.filename().string());
        return true;
    });

    CHECK(names.size() == 1, "symlink: only real file visited");
    CHECK(names[0] == "real.txt", "symlink: symlink was skipped");
}

static void test_inaccessible_directory_skipped() {
    fs::path tmp = setupTempDir();
    fs::path locked = tmp / "locked";
    fs::create_directories(locked);
    writeFile(locked / "hidden.txt");
    writeFile(tmp / "visible.txt");
    fs::permissions(locked, fs::perms::none);

    FileTraverser t;
    std::vector<std::string> names;
    t.traverse(tmp, [&](const fs::path& p) {
        names.push_back(p.filename().string());
        return true;
    });

    fs::permissions(locked, fs::perms::all); // restore so cleanup works
    CHECK(names.size() == 1, "inaccessible dir: only visible file visited");
    CHECK(names[0] == "visible.txt", "inaccessible dir: locked dir skipped");
}

static void test_callback_stop() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "a.txt");
    writeFile(tmp / "b.txt");
    writeFile(tmp / "c.txt");

    FileTraverser t;
    std::vector<std::string> names;
    t.traverse(tmp, [&](const fs::path& p) {
        names.push_back(p.filename().string());
        return names.size() < 2; // stop after 2 files
    });

    CHECK(names.size() == 2, "callback stop: traversal stopped after 2 files");
}

static void test_deterministic_order() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "c.txt");
    writeFile(tmp / "a.txt");
    writeFile(tmp / "b.txt");

    FileTraverser t;
    auto collect = [&]() {
        std::vector<std::string> names;
        t.traverse(tmp, [&](const fs::path& p) {
            names.push_back(p.filename().string());
            return true;
        });
        return names;
    };

    CHECK(collect() == collect(), "deterministic: same order on repeated runs");
}

// ── FileMetadataProvider tests ───────────────────────────────────────────────

static void test_metadata_reads_correctly() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "f.txt", "hello");

    FileMetadataProvider mp;
    auto meta = mp.read(tmp / "f.txt");

    CHECK(meta.size == 5, "metadata: size is correct");
    CHECK(meta.inode > 0, "metadata: inode is non-zero");
    CHECK(meta.deviceId > 0, "metadata: device id is non-zero");
    CHECK(!meta.normalizedPath.is_relative(), "metadata: path is absolute");
}

static void test_same_file_state_unchanged() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "f.txt", "hello");

    FileMetadataProvider mp;
    auto before = mp.read(tmp / "f.txt");
    auto after  = mp.read(tmp / "f.txt");

    CHECK(mp.sameFileState(before, after), "same state: unchanged file matches");
}

static void test_same_file_state_size_changed() {
    fs::path tmp = setupTempDir();
    writeFile(tmp / "f.txt", "hi");

    FileMetadataProvider mp;
    auto before = mp.read(tmp / "f.txt");
    writeFile(tmp / "f.txt", "much longer content now");
    auto after = mp.read(tmp / "f.txt");

    CHECK(!mp.sameFileState(before, after), "same state: size change detected");
}

static void test_missing_file_throws() {
    FileMetadataProvider mp;
    bool threw = false;
    try {
        mp.read("/tmp/this_file_does_not_exist_scanner_test_xyz");
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "missing file: throws exception");
}

// ────────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== FileTraverser tests ===\n";
    test_single_file();
    test_alphabetical_order();
    test_empty_directory();
    test_symlink_skipped();
    test_inaccessible_directory_skipped();
    test_callback_stop();
    test_deterministic_order();

    std::cout << "\n=== FileMetadataProvider tests ===\n";
    test_metadata_reads_correctly();
    test_same_file_state_unchanged();
    test_same_file_state_size_changed();
    test_missing_file_throws();

    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return failed > 0 ? 1 : 0;
}
