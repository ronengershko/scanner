#include "scan/ScanService.h"
#include "database/CacheRepository.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;

// Set SCANNER_FILE_DELAY_MS=<ms> to slow scanning for stop/resume testing.
static int fileDelayMs() {
    const char* v = std::getenv("SCANNER_FILE_DELAY_MS");
    return v ? std::stoi(v) : 0;
}

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
                         QuarantineService& quarantineService,
                         Logger& logger)
    : m_traverser(traverser), m_scanner(scanner), m_metaProvider(metaProvider),
      m_sessionRepo(sessionRepo), m_signatureService(signatureService),
      m_cacheRepo(cacheRepo), m_quarantineService(quarantineService),
      m_logger(logger) {}

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
        if (cached->verdict == "malicious") {
            counters.malicious++;
            m_logger.warning("Malicious (cached): " + pathStr);
            std::cout << "MALICIOUS: " << pathStr << "\n";
            try {
                m_quarantineService.quarantine(file, meta, std::nullopt);
            } catch (const std::exception& e) {
                m_logger.error("Quarantine failed: " + pathStr + ": " + e.what());
            }
        }
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
            try {
                m_quarantineService.quarantine(file, meta, result.matchedSignatureId);
            } catch (const std::exception& e) {
                m_logger.error("Quarantine failed: " + pathStr + ": " + e.what());
            }
        }
    }

    m_sessionRepo.updateCheckpoint(sessionId, pathStr);
}

static void printSummary(bool stopped, const std::string& path,
                         int64_t scanned, int64_t cacheHits,
                         int64_t malicious, int64_t errors, double elapsed) {
    std::cout << (stopped ? "Scan stopped" : "Scan completed") << "\n"
              << "Path: " << path << "\n"
              << "Files scanned: " << scanned << "\n"
              << "Cache hits: " << cacheHits << "\n"
              << "Malicious files: " << malicious << "\n"
              << "Errors: " << errors << "\n"
              << "Duration: " << std::fixed << std::setprecision(1) << elapsed << " seconds\n";
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
    int delayMs = fileDelayMs();

    try {
        m_traverser.traverse(canonical, [&](const fs::path& file) -> bool {
            if (m_sessionRepo.getStatus(sessionId) == "stop_requested") {
                m_sessionRepo.updateStatus(sessionId, "stopped");
                stopped = true;
                return false;
            }
            processFile(file, sigs, sigVersion, sessionId, counters);
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
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

    printSummary(stopped, canonical.string(), counters.scanned, counters.cacheHits,
                 counters.malicious, counters.errors, elapsed);
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
    if (m_sessionRepo.findRunning()) {
        std::cerr << "A scan is already running.\n";
        return 1;
    }

    auto session = m_sessionRepo.findStopped();
    if (!session) {
        std::cerr << "No stopped scan to resume.\n";
        return 1;
    }

    m_sessionRepo.updateStatus(session->id, "running");
    m_logger.info("Scan resumed path=" + session->canonicalPath +
                  " checkpoint=" + session->lastCompletedPath);

    auto sigs = m_signatureService.loadForScanning();
    int64_t sigVersion = m_signatureService.getVersion();
    std::string checkpoint = session->lastCompletedPath;
    int64_t sessionId = session->id;

    Counters counters;
    auto startTime = std::chrono::steady_clock::now();
    bool stopped = false;
    int delayMs = fileDelayMs();

    try {
        m_traverser.traverse(fs::path(session->canonicalPath), [&](const fs::path& file) -> bool {
            // Skip files already processed before the checkpoint
            std::string filePath = fs::weakly_canonical(file).string();
            if (!checkpoint.empty() && filePath <= checkpoint)
                return true;

            if (m_sessionRepo.getStatus(sessionId) == "stop_requested") {
                m_sessionRepo.updateStatus(sessionId, "stopped");
                stopped = true;
                return false;
            }

            processFile(file, sigs, sigVersion, sessionId, counters);
            if (delayMs > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            return true;
        });

        if (!stopped)
            m_sessionRepo.updateStatus(sessionId, "completed");
    } catch (const std::exception& e) {
        m_sessionRepo.updateStatus(sessionId, "failed");
        m_logger.error("Resume failed: " + std::string(e.what()));
        throw;
    }

    m_sessionRepo.updateCounters(sessionId, counters.scanned, counters.cacheHits,
                                 counters.malicious, counters.errors);

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startTime).count();

    printSummary(stopped, session->canonicalPath, counters.scanned, counters.cacheHits,
                 counters.malicious, counters.errors, elapsed);
    m_logger.info(std::string(stopped ? "Scan stopped" : "Scan completed") +
                  " (resumed) scanned=" + std::to_string(counters.scanned));
    return 0;
}
