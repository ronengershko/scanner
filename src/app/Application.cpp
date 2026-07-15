#include "app/Application.h"
#include "cli/Command.h"
#include "cli/CommandLineParser.h"
#include <iostream>

Application::Application(AppConfig config, Logger& logger, Database& database)
    : m_config(std::move(config)), m_logger(logger), m_database(database) {
    m_config.createRequiredDirectories();
    m_logger.info("Scanner started");
}

int Application::run(int argc, char* argv[]) {
    CommandLineParser parser;
    Command cmd;

    try {
        cmd = parser.parse(argc, argv);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::string what = e.what();
        m_logger.error("Invalid command: " + what.substr(0, what.find('\n')));
        return 1;
    }

    switch (cmd.type) {
        case CommandType::ScanPath:
            std::cout << "Scan path command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::ScanAll:
            std::cout << "Scan all command received\n";
            break;
        case CommandType::Stop:
            std::cout << "Stop command received\n";
            break;
        case CommandType::Resume:
            std::cout << "Resume command received\n";
            break;
        case CommandType::SignatureList:
            std::cout << "Signature list command received\n";
            break;
        case CommandType::SignatureAdd:
            std::cout << "Signature add command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::SignatureRemove:
            std::cout << "Signature remove command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::ExclusionList:
            std::cout << "Exclusion list command received\n";
            break;
        case CommandType::ExclusionAdd:
            std::cout << "Exclusion add command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::ExclusionRemove:
            std::cout << "Exclusion remove command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::QuarantineList:
            std::cout << "Quarantine list command received\n";
            break;
        case CommandType::QuarantineRestore:
            std::cout << "Quarantine restore command received: " << cmd.argument.value() << "\n";
            break;
        case CommandType::QuarantineDelete:
            std::cout << "Quarantine delete command received: " << cmd.argument.value() << "\n";
            break;
    }

    return 0;
}
