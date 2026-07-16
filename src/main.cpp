#include "app/AppConfig.h"
#include "app/Application.h"
#include "database/CacheRepository.h"
#include "database/Database.h"
#include "database/ExclusionRepository.h"
#include "database/QuarantineRepository.h"
#include "database/ScanSessionRepository.h"
#include "database/SignatureRepository.h"
#include "exclusions/ExclusionService.h"
#include "logging/Logger.h"
#include "quarantine/QuarantineService.h"
#include "scan/FileMetadataProvider.h"
#include "scan/FileScanner.h"
#include "scan/FileTraverser.h"
#include "scan/ScanService.h"
#include "signatures/SignatureService.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        AppConfig config = AppConfig::load();
        Logger logger(config.logPath);
        Database database(config.databasePath);
        database.initializeSchema();

        SignatureRepository   signatureRepo(database);
        ScanSessionRepository sessionRepo(database);
        CacheRepository       cacheRepo(database);
        QuarantineRepository  quarantineRepo(database);
        ExclusionRepository   exclusionRepo(database);

        SignatureService  signatureService(signatureRepo, logger);
        QuarantineService quarantineService(quarantineRepo, logger, config.quarantineDirectory);
        ExclusionService  exclusionService(exclusionRepo, logger);

        // Ensure scanner-data is always excluded so the scanner never scans itself.
        namespace fs = std::filesystem;
        std::string dataDirStr = fs::weakly_canonical(config.databasePath.parent_path()).string();
        bool dataDirExcluded = false;
        for (const auto& e : exclusionRepo.loadAll())
            if (e.path == dataDirStr) { dataDirExcluded = true; break; }
        if (!dataDirExcluded)
            exclusionRepo.add(dataDirStr, true);

        FileTraverser        traverser;
        FileScanner          scanner;
        FileMetadataProvider metaProvider;

        ScanService scanService(traverser, scanner, metaProvider,
                                sessionRepo, signatureService, cacheRepo,
                                quarantineService, exclusionService, logger);

        Application app(std::move(config), logger, scanService,
                        signatureService, quarantineService, exclusionService);
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
