#include "app/Application.h"
#include "cli/Command.h"
#include "cli/CommandLineParser.h"
#include <iostream>
#include <stdexcept>

Application::Application(AppConfig config, Logger& logger,
                         ScanService& scanService, SignatureService& signatureService,
                         QuarantineService& quarantineService)
    : m_config(std::move(config)), m_logger(logger),
      m_scanService(scanService), m_signatureService(signatureService),
      m_quarantineService(quarantineService)
{
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

    try {
        switch (cmd.type) {
            case CommandType::ScanPath:
                return m_scanService.scanPath(cmd.argument.value());
            case CommandType::ScanAll:
                return m_scanService.scanAll(m_config.fullScanRoot);
            case CommandType::Stop:
                return m_scanService.requestStop();
            case CommandType::Resume:
                return m_scanService.resume();

            case CommandType::SignatureList:
                m_signatureService.list();
                break;
            case CommandType::SignatureAdd:
                m_signatureService.add(cmd.argument.value());
                break;
            case CommandType::SignatureRemove:
                m_signatureService.remove(std::stoll(cmd.argument.value()));
                break;

            case CommandType::ExclusionList:
                std::cout << "Exclusion list command received\n";
                break;
            case CommandType::ExclusionAdd:
                std::cout << "Exclusion add: " << cmd.argument.value() << "\n";
                break;
            case CommandType::ExclusionRemove:
                std::cout << "Exclusion remove: " << cmd.argument.value() << "\n";
                break;

            case CommandType::QuarantineList:
                m_quarantineService.list();
                break;
            case CommandType::QuarantineRestore:
                m_quarantineService.restore(cmd.argument.value());
                std::cout << "Restored.\n";
                break;
            case CommandType::QuarantineDelete:
                m_quarantineService.remove(cmd.argument.value());
                std::cout << "Deleted.\n";
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        m_logger.error(e.what());
        return 1;
    }

    return 0;
}
