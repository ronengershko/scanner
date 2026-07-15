#pragma once
#include "app/AppConfig.h"
#include "database/Database.h"
#include "logging/Logger.h"
#include "signatures/SignatureService.h"

class Application {
public:
    Application(AppConfig config, Logger& logger, Database& database,
                SignatureService& signatureService);
    int run(int argc, char* argv[]);

private:
    AppConfig m_config;
    Logger& m_logger;
    Database& m_database;
    SignatureService& m_signatureService;
};
