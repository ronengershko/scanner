#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class Verdict {
    Clean,
    Malicious,
    Error
};

struct Signature {
    int64_t id;
    std::vector<std::byte> value;
};

struct ScanResult {
    Verdict verdict;
    std::optional<int64_t> matchedSignatureId;
    std::optional<std::string> errorMessage;
};
