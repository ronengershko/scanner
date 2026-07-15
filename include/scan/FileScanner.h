#pragma once
#include "scan/ScanTypes.h"
#include <filesystem>
#include <vector>

class FileScanner {
public:
    ScanResult scan(const std::filesystem::path& file,
                    const std::vector<Signature>& signatures) const;
};
