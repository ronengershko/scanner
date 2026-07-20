#include "scan/ScanService.h"
#include "database/CacheRepository.h"
#include "exclusions/ExclusionService.h"
#include "scan/FileWatcher.h"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

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
                         QuarantineService& quarantineService,
                         ExclusionService& exclusionService,
                         MonitorSessionRepository& monitorRepo,
                         Logger& logger)
    : m_traverser(traverser), m_scanner(scanner), m_metaProvider(metaProvider),
      m_sessionRepo(sessionRepo), m_signatureService(signatureService),
      m_cacheRepo(cacheRepo), m_quarantineService(quarantineService),
      m_exclusionService(exclusionService), m_monitorRepo(monitorRepo),
      m_logger(logger) {}


void ScanService::processFile(const fs::path& file,
                               const std::vector<Signature>& sigs,
                               int64_t sigVersion,
                               int64_t sessionId,
                               Counters& counters)
{
    if (m_exclusionService.isExcluded(file)) {
        counters.excluded++;
        return;
    }

    FileMetadata meta;
    try {
        meta = m_metaProvider.read(file);
    } catch (const std::exception& e) {
        m_logger.error("Cannot read metadata: " + file.string() + ": " + e.what());
        counters.errors++;
        return;
    }

    std::string pathStr = meta.normalizedPath.string();

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
        if (sessionId > 0)
            m_sessionRepo.updateCheckpoint(sessionId, pathStr);
        return;
    }

    ScanResult result = m_scanner.scan(file, sigs);

    std::optional<FileMetadata> stableMeta;
    try {
        FileMetadata metaAfter = m_metaProvider.read(file);
        if (m_metaProvider.sameFileState(meta, metaAfter)) {
            stableMeta = meta;
        } else {
            FileMetadata metaBefore2 = m_metaProvider.read(file);
            result = m_scanner.scan(file, sigs);
            FileMetadata metaAfter2 = m_metaProvider.read(file);
            if (m_metaProvider.sameFileState(metaBefore2, metaAfter2))
                stableMeta = metaBefore2;
        }
    } catch (...) {}

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

    if (sessionId > 0)
        m_sessionRepo.updateCheckpoint(sessionId, pathStr);
}


static void printSummary(bool stopped, const std::string& path,
                         int64_t scanned, int64_t cacheHits,
                         int64_t malicious, int64_t excluded, int64_t errors, double elapsed) {
    std::cout << (stopped ? "Scan stopped" : "Scan completed") << "\n"
              << "Path: " << path << "\n"
              << "Files scanned: " << scanned << "\n"
              << "Cache hits: " << cacheHits << "\n"
              << "Malicious files: " << malicious << "\n"
              << "Excluded files: " << excluded << "\n"
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
                                 counters.malicious, counters.excluded, counters.errors);

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startTime).count();

    printSummary(stopped, canonical.string(), counters.scanned, counters.cacheHits,
                 counters.malicious, counters.excluded, counters.errors, elapsed);
    m_logger.info(std::string(stopped ? "Scan stopped" : "Scan completed") +
                  " scanned=" + std::to_string(counters.scanned) +
                  " malicious=" + std::to_string(counters.malicious));
    return 0;
}


int ScanService::scanPath(const fs::path& path) {
    return runScan(path, "path");
}

int ScanService::scanAll() {
    std::string root = m_sessionRepo.getScanRoot();
    if (root.empty())
        throw std::runtime_error("Scan root not set. Use 'scanner config set-root <path>'.");
    return runScan(fs::path(root), "all");
}


void ScanService::setScanRoot(const fs::path& path) {
    fs::path p = fs::weakly_canonical(path);
    m_sessionRepo.setScanRoot(p.string());
    m_logger.info("Scan root set to: " + p.string());
    std::cout << "Scan root: " << p.string() << "\n";
}


void ScanService::watchAdd(const fs::path& path) {
    fs::path p = fs::weakly_canonical(path);
    m_monitorRepo.addWatchPath(p.string());
    m_logger.info("Watch path added: " + p.string());
    std::cout << "Watch path added: " << p.string() << "\n";
    notifyMonitor();
}

void ScanService::watchRemove(int64_t id) {
    m_monitorRepo.removeWatchPath(id);
    std::cout << "Watch path removed.\n";
    notifyMonitor();
}

void ScanService::watchList() {
    auto paths = m_monitorRepo.loadAllWatchPaths();
    if (paths.empty()) {
        std::cout << "No watch paths configured.\n";
        return;
    }
    std::cout << "ID  Path\n";
    for (const auto& wp : paths)
        std::cout << wp.id << "   " << wp.path << "\n";
}

void ScanService::notifyMonitor() {
    auto running = m_monitorRepo.findRunning();
    if (running && kill(static_cast<pid_t>(running->pid), SIGUSR1) == 0)
        std::cout << "Monitor reloaded.\n";
}


int ScanService::monitor() {
    // Kill any existing monitor
    auto existing = m_monitorRepo.findRunning();
    if (existing) {
        if (kill(static_cast<pid_t>(existing->pid), 0) == 0)
            kill(static_cast<pid_t>(existing->pid), SIGTERM);
        m_monitorRepo.updateStatus(existing->id, "stopped");
    }

    int64_t sessionId = m_monitorRepo.create(static_cast<int64_t>(getpid()));
    m_logger.info("Monitor started pid=" + std::to_string(getpid()));

    auto sigs = m_signatureService.loadForScanning();
    int64_t sigVersion = m_signatureService.getVersion();
    Counters counters;

    while (true) {
        auto watchPaths = m_monitorRepo.loadAllWatchPaths();
        if (watchPaths.empty()) {
            std::cerr << "No watch paths set. Use 'scanner watch add <path>'.\n";
            m_monitorRepo.updateStatus(sessionId, "stopped");
            return 1;
        }

        std::vector<std::string> paths;
        for (const auto& wp : watchPaths)
            paths.push_back(wp.path);

        FileWatcher watcher(paths, [&](const std::string& path) {
            processFile(fs::path(path), sigs, sigVersion, 0, counters);
        });

        bool reload = watcher.start();
        if (!reload) break;
        m_logger.info("Monitor reloading watch paths");
    }

    m_monitorRepo.updateStatus(sessionId, "stopped");
    m_logger.info("Monitor stopped");
    return 0;
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
    try {
        m_traverser.traverse(fs::path(session->canonicalPath), [&](const fs::path& file) -> bool {
            fs::path filePath = fs::weakly_canonical(file);
            if (!checkpoint.empty() && filePath <= fs::path(checkpoint))
                return true;

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
        m_logger.error("Resume failed: " + std::string(e.what()));
        throw;
    }

    m_sessionRepo.updateCounters(sessionId, counters.scanned, counters.cacheHits,
                                 counters.malicious, counters.excluded, counters.errors);

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startTime).count();

    printSummary(stopped, session->canonicalPath, counters.scanned, counters.cacheHits,
                 counters.malicious, counters.excluded, counters.errors, elapsed);
    m_logger.info(std::string(stopped ? "Scan stopped" : "Scan completed") +
                  " (resumed) scanned=" + std::to_string(counters.scanned));
    return 0;
}


void ScanService::sessionList() {
    auto sessions = m_sessionRepo.listRecent(10);
    if (sessions.empty()) {
        std::cout << "No scan sessions.\n";
        return;
    }
    std::cout << "ID   Type  Status     Scanned  Malicious  Path\n";
    for (const auto& s : sessions) {
        std::cout << s.id << "    "
                  << s.scanType << "  "
                  << s.status << "  "
                  << s.scannedFiles << "  "
                  << s.maliciousFiles << "  "
                  << s.canonicalPath << "\n";
    }
}

void ScanService::monitorSessionList() {
    auto sessions = m_monitorRepo.listRecent(10);
    if (sessions.empty()) {
        std::cout << "No monitor sessions.\n";
        return;
    }
    std::cout << "ID   PID    Status   Started              Stopped\n";
    for (const auto& s : sessions) {
        std::cout << s.id << "    "
                  << s.pid << "  "
                  << s.status << "  "
                  << s.startedAt << "  "
                  << s.stoppedAt << "\n";
    }
}
