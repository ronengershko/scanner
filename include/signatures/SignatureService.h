#pragma once
#include "database/SignatureRepository.h"
#include "logging/Logger.h"
#include "scan/ScanTypes.h"
#include <string>
#include <vector>

class SignatureService {
public:
    SignatureService(SignatureRepository& repo, Logger& logger);

    void list() const;
    void add(const std::string& value);
    void remove(int64_t id);

    std::vector<Signature> loadForScanning() const;
    int64_t getVersion() const;

private:
    SignatureRepository& m_repo;
    Logger& m_logger;
};
