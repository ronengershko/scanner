#include "app/AppConfig.h"
#include "app/Application.h"
#include "logging/Logger.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        AppConfig config = AppConfig::load();
        Logger logger(config.logPath);
        Application app(std::move(config), logger);
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
