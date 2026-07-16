#pragma once
#include "exclusions/ExclusionService.h"
#include "logging/Logger.h"
#include "quarantine/QuarantineService.h"
#include "scan/ScanService.h"
#include "signatures/SignatureService.h"

class Application {
public:
    Application(Logger& logger,
                ScanService& scanService, SignatureService& signatureService,
                QuarantineService& quarantineService,
                ExclusionService& exclusionService);
    int run(int argc, char* argv[]);

private:
    Logger&             m_logger;
    ScanService&        m_scanService;
    SignatureService&   m_signatureService;
    QuarantineService&  m_quarantineService;
    ExclusionService&   m_exclusionService;
};
