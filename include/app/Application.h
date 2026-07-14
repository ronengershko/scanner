#pragma once
#include "app/AppConfig.h"
#include "logging/Logger.h"

class Application {
public:
    Application(AppConfig config, Logger& logger);
    int run(int argc, char* argv[]);

private:
    AppConfig m_config;
    Logger& m_logger;
};
