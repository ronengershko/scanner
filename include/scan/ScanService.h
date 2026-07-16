#pragma once
#include "database/CacheRepository.h"
#include "database/ScanSessionRepository.h"
#include "exclusions/ExclusionService.h"
#include "logging/Logger.h"
#include "quarantine/QuarantineService.h"
#include "scan/FileMetadataProvider.h"
#include "scan/FileScanner.h"
#include "scan/FileTraverser.h"
#include "scan/ScanTypes.h"
#include "signatures/SignatureService.h"
#include <filesystem>
#include <vector>

class ScanService {
public:
    ScanService(FileTraverser& traverser, FileScanner& scanner,
                FileMetadataProvider& metaProvider,
                ScanSessionRepository& sessionRepo,
                SignatureService& signatureService,
                CacheRepository& cacheRepo,
                QuarantineService& quarantineService,
                ExclusionService& exclusionService,
                Logger& logger);

    int scanPath(const std::filesystem::path& path);
    int scanAll(const std::filesystem::path& fullScanRoot);
    int requestStop();
    int resume();

private:
    struct Counters {
        int64_t scanned{0}, cacheHits{0}, malicious{0}, errors{0};
    };

    int runScan(const std::filesystem::path& path, const std::string& scanType);

    void processFile(const std::filesystem::path& file,
                     const std::vector<Signature>& sigs,
                     int64_t sigVersion,
                     int64_t sessionId,
                     Counters& counters);

    FileTraverser&         m_traverser;
    FileScanner&           m_scanner;
    FileMetadataProvider&  m_metaProvider;
    ScanSessionRepository& m_sessionRepo;
    SignatureService&      m_signatureService;
    CacheRepository&       m_cacheRepo;
    QuarantineService&     m_quarantineService;
    ExclusionService&      m_exclusionService;
    Logger&                m_logger;
};
