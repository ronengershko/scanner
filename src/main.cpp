#include "app/AppConfig.h"
#include "app/Application.h"
#include "database/CacheRepository.h"
#include "database/Database.h"
#include "database/ScanSessionRepository.h"
#include "database/SignatureRepository.h"
#include "logging/Logger.h"
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

        SignatureRepository  signatureRepo(database);
        ScanSessionRepository sessionRepo(database);
        CacheRepository      cacheRepo(database);

        SignatureService signatureService(signatureRepo, logger);

        FileTraverser        traverser;
        FileScanner          scanner;
        FileMetadataProvider metaProvider;

        ScanService scanService(traverser, scanner, metaProvider,
                                sessionRepo, signatureService, cacheRepo, logger);

        Application app(std::move(config), logger, scanService, signatureService);
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
