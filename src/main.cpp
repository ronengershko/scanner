#include "app/AppConfig.h"
#include "app/Application.h"
#include "database/Database.h"
#include "logging/Logger.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        AppConfig config = AppConfig::load();
        Logger logger(config.logPath);
        Database database(config.databasePath);
        database.initializeSchema();
        Application app(std::move(config), logger, database);
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
