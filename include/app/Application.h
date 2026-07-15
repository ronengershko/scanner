#pragma once
#include "app/AppConfig.h"
#include "logging/Logger.h"
#include "quarantine/QuarantineService.h"
#include "scan/ScanService.h"
#include "signatures/SignatureService.h"

class Application {
public:
    Application(AppConfig config, Logger& logger,
                ScanService& scanService, SignatureService& signatureService,
                QuarantineService& quarantineService);
    int run(int argc, char* argv[]);

private:
    AppConfig          m_config;
    Logger&            m_logger;
    ScanService&       m_scanService;
    SignatureService&  m_signatureService;
    QuarantineService& m_quarantineService;
};
