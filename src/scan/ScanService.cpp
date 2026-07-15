#include "scan/ScanService.h"
#include "database/CacheRepository.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

static std::string verdictStr(Verdict v) {
    switch (v) {
        case Verdict::Clean:    return "clean";
        case Verdict::Malicious: return "malicious";
        default:                return "error";
    }
}

ScanService::ScanService(FileTraverser& traverser, FileScanner& scanner,
                         FileMetadataProvider& metaProvider,
                         ScanSessionRepository& sessionRepo,
                         SignatureService& signatureService,
                         CacheRepository& cacheRepo,
                         Logger& logger)
    : m_traverser(traverser), m_scanner(scanner), m_metaProvider(metaProvider),
      m_sessionRepo(sessionRepo), m_signatureService(signatureService),
      m_cacheRepo(cacheRepo), m_logger(logger) {}

void ScanService::processFile(const fs::path& file,
                               const std::vector<Signature>& sigs,
                               int64_t sigVersion,
                               int64_t sessionId,
                               Counters& counters)
{
    FileMetadata meta;
    try {
        meta = m_metaProvider.read(file);
    } catch (const std::exception& e) {
        m_logger.error("Cannot read metadata: " + file.string() + ": " + e.what());
        counters.errors++;
        return;
    }

    std::string pathStr = meta.normalizedPath.string();

    // Cache check
    auto cached = m_cacheRepo.lookup(pathStr);
    if (cached && CacheRepository::isValidHit(*cached, meta, sigVersion)) {
        counters.cacheHits++;
        m_sessionRepo.updateCheckpoint(sessionId, pathStr);
        return;
    }

    // Scan
    ScanResult result = m_scanner.scan(file, sigs);

    // Stability check: compare metadata before and after scan
    std::optional<FileMetadata> stableMeta;
    try {
        FileMetadata metaAfter = m_metaProvider.read(file);
        if (m_metaProvider.sameFileState(meta, metaAfter)) {
            stableMeta = meta;
        } else {
            // File changed during scan — retry once with fresh metadata
            FileMetadata metaBefore2 = m_metaProvider.read(file);
            result = m_scanner.scan(file, sigs);
            FileMetadata metaAfter2 = m_metaProvider.read(file);
            if (m_metaProvider.sameFileState(metaBefore2, metaAfter2))
                stableMeta = metaBefore2;
        }
    } catch (...) {}

    // Cache stable, non-error results
    if (stableMeta && result.verdict != Verdict::Error) {
        m_cacheRepo.upsert({pathStr,
            static_cast<int64_t>(stableMeta->deviceId),
            static_cast<int64_t>(stableMeta->inode),
            static_cast<int64_t>(stableMeta->size),
            stableMeta->modificationTime,
            sigVersion,
            verdictStr(result.verdict)});
    }

    if (result.verdict == Verdict::Error) {
        counters.errors++;
        m_logger.error("Scan error: " + pathStr +
                       (result.errorMessage ? ": " + *result.errorMessage : ""));
    } else {
        counters.scanned++;
        if (result.verdict == Verdict::Malicious) {
            counters.malicious++;
            m_logger.warning("Malicious: " + pathStr);
            std::cout << "MALICIOUS: " << pathStr << "\n";
        }
    }

    m_sessionRepo.updateCheckpoint(sessionId, pathStr);
}

int ScanService::runScan(const fs::path& path, const std::string& scanType) {
    if (m_sessionRepo.findRunning()) {
        std::cerr << "A scan is already running.\n";
        return 1;
    }

    fs::path canonical = fs::weakly_canonical(path);
    auto sigs = m_signatureService.loadForScanning();
    int64_t sigVersion = m_signatureService.getVersion();

    int64_t sessionId = m_sessionRepo.create(path.string(), canonical.string(), scanType);
    m_logger.info("Scan started path=" + canonical.string());

    Counters counters;
    auto startTime = std::chrono::steady_clock::now();
    bool stopped = false;

    try {
        m_traverser.traverse(canonical, [&](const fs::path& file) -> bool {
            if (m_sessionRepo.getStatus(sessionId) == "stop_requested") {
                m_sessionRepo.updateStatus(sessionId, "stopped");
                stopped = true;
                return false;
            }
            processFile(file, sigs, sigVersion, sessionId, counters);
            return true;
        });

        if (!stopped)
            m_sessionRepo.updateStatus(sessionId, "completed");
    } catch (const std::exception& e) {
        m_sessionRepo.updateStatus(sessionId, "failed");
        m_logger.error("Scan failed: " + std::string(e.what()));
        throw;
    }

    m_sessionRepo.updateCounters(sessionId, counters.scanned, counters.cacheHits,
                                 counters.malicious, counters.errors);

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startTime).count();

    std::cout << (stopped ? "Scan stopped" : "Scan completed") << "\n"
              << "Path: " << canonical.string() << "\n"
              << "Files scanned: " << counters.scanned << "\n"
              << "Cache hits: " << counters.cacheHits << "\n"
              << "Malicious files: " << counters.malicious << "\n"
              << "Errors: " << counters.errors << "\n"
              << "Duration: " << std::fixed << std::setprecision(1) << elapsed << " seconds\n";

    m_logger.info(std::string(stopped ? "Scan stopped" : "Scan completed") +
                  " scanned=" + std::to_string(counters.scanned) +
                  " malicious=" + std::to_string(counters.malicious));
    return 0;
}

int ScanService::scanPath(const fs::path& path) {
    return runScan(path, "path");
}

int ScanService::scanAll(const fs::path& fullScanRoot) {
    return runScan(fullScanRoot, "all");
}

int ScanService::requestStop() {
    auto session = m_sessionRepo.findRunning();
    if (!session) {
        std::cerr << "No running scan to stop.\n";
        return 1;
    }
    m_sessionRepo.updateStatus(session->id, "stop_requested");
    std::cout << "Stop requested.\n";
    return 0;
}

int ScanService::resume() {
    std::cerr << "Resume not yet implemented (step 10).\n";
    return 1;
}
