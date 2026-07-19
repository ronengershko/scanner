#include <gtest/gtest.h>
#include "cli/Command.h"
#include "cli/CommandLineParser.h"
#include <vector>
#include <string>

static Command parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("scanner"));
    for (auto& a : args) argv.push_back(a.data());
    return CommandLineParser{}.parse(static_cast<int>(argv.size()), argv.data());
}

TEST(CLI, ScanPath) {
    auto c = parse({"scan", "--path", "/tmp/foo"});
    EXPECT_EQ(c.type, CommandType::ScanPath);
    EXPECT_EQ(c.argument, "/tmp/foo");
}

TEST(CLI, ScanAll) {
    auto c = parse({"scan", "--all"});
    EXPECT_EQ(c.type, CommandType::ScanAll);
    EXPECT_FALSE(c.argument.has_value());
}

TEST(CLI, Stop) {
    EXPECT_EQ(parse({"stop"}).type, CommandType::Stop);
}

TEST(CLI, Resume) {
    EXPECT_EQ(parse({"resume"}).type, CommandType::Resume);
}

TEST(CLI, SignaturesList) {
    EXPECT_EQ(parse({"signatures", "list"}).type, CommandType::SignatureList);
}

TEST(CLI, SignaturesAdd) {
    auto c = parse({"signatures", "add", "deadbeef"});
    EXPECT_EQ(c.type, CommandType::SignatureAdd);
    EXPECT_EQ(c.argument, "deadbeef");
}

TEST(CLI, SignaturesRemove) {
    auto c = parse({"signatures", "remove", "3"});
    EXPECT_EQ(c.type, CommandType::SignatureRemove);
    EXPECT_EQ(c.argument, "3");
}

TEST(CLI, ExclusionsList) {
    EXPECT_EQ(parse({"exclusions", "list"}).type, CommandType::ExclusionList);
}

TEST(CLI, ExclusionsAdd) {
    auto c = parse({"exclusions", "add", "/tmp/safe"});
    EXPECT_EQ(c.type, CommandType::ExclusionAdd);
    EXPECT_EQ(c.argument, "/tmp/safe");
}

TEST(CLI, ExclusionsRemove) {
    auto c = parse({"exclusions", "remove", "2"});
    EXPECT_EQ(c.type, CommandType::ExclusionRemove);
    EXPECT_EQ(c.argument, "2");
}

TEST(CLI, QuarantineList) {
    EXPECT_EQ(parse({"quarantine", "list"}).type, CommandType::QuarantineList);
}

TEST(CLI, QuarantineRestore) {
    auto c = parse({"quarantine", "restore", "abc-123"});
    EXPECT_EQ(c.type, CommandType::QuarantineRestore);
    EXPECT_EQ(c.argument, "abc-123");
}

TEST(CLI, QuarantineDelete) {
    auto c = parse({"quarantine", "delete", "abc-123"});
    EXPECT_EQ(c.type, CommandType::QuarantineDelete);
    EXPECT_EQ(c.argument, "abc-123");
}

TEST(CLI, ConfigSetRoot) {
    auto c = parse({"config", "set-root", "/Users/ronen"});
    EXPECT_EQ(c.type, CommandType::ConfigSetRoot);
    EXPECT_EQ(c.argument, "/Users/ronen");
}

TEST(CLI, NoArgsThrows) {
    EXPECT_THROW(parse({}), std::invalid_argument);
}

TEST(CLI, UnknownCommandThrows) {
    EXPECT_THROW(parse({"bogus"}), std::invalid_argument);
}

TEST(CLI, ScanMissingPathThrows) {
    EXPECT_THROW(parse({"scan", "--path"}), std::invalid_argument);
}

TEST(CLI, StopWithArgsThrows) {
    EXPECT_THROW(parse({"stop", "extra"}), std::invalid_argument);
}
