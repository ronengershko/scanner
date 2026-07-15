#include "scan/FileScanner.h"
#include <algorithm>
#include <fstream>

static const size_t CHUNK_SIZE = 65536;

ScanResult FileScanner::scan(const std::filesystem::path& file,
                             const std::vector<Signature>& signatures) const {
    if (signatures.empty())
        return {Verdict::Clean, std::nullopt, std::nullopt};

    std::ifstream f(file, std::ios::binary);
    if (!f.is_open())
        return {Verdict::Error, std::nullopt, "Cannot open: " + file.string()};

    size_t maxSigLen = 0;
    for (const auto& sig : signatures)
        maxSigLen = std::max(maxSigLen, sig.value.size());

    size_t overlap = maxSigLen > 1 ? maxSigLen - 1 : 0;

    std::vector<std::byte> window;
    std::vector<std::byte> chunk(CHUNK_SIZE);

    while (f.read(reinterpret_cast<char*>(chunk.data()), CHUNK_SIZE) || f.gcount() > 0) {
        size_t n = static_cast<size_t>(f.gcount());
        window.insert(window.end(), chunk.begin(), chunk.begin() + n);

        for (const auto& sig : signatures) {
            auto it = std::search(window.begin(), window.end(),
                                  sig.value.begin(), sig.value.end());
            if (it != window.end())
                return {Verdict::Malicious, sig.id, std::nullopt};
        }

        if (window.size() > overlap)
            window.erase(window.begin(), window.begin() + (window.size() - overlap));
    }

    if (f.bad())
        return {Verdict::Error, std::nullopt, "Read error: " + file.string()};

    return {Verdict::Clean, std::nullopt, std::nullopt};
}
