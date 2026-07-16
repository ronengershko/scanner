#include "cli/CommandLineParser.h"
#include <stdexcept>
#include <string>

static const char* USAGE =
    "Usage:\n"
    "  scanner scan --path <path>\n"
    "  scanner scan --all\n"
    "  scanner stop\n"
    "  scanner resume\n"
    "  scanner signatures list\n"
    "  scanner signatures add <value>\n"
    "  scanner signatures remove <id>\n"
    "  scanner exclusions list\n"
    "  scanner exclusions add <path>\n"
    "  scanner exclusions remove <id>\n"
    "  scanner quarantine list\n"
    "  scanner quarantine restore <id>\n"
    "  scanner quarantine delete <id>\n"
    "  scanner config set-root <path>\n";

Command CommandLineParser::parse(int argc, char* argv[]) const {
    if (argc < 2)
        throw std::invalid_argument(std::string("No command given.\n") + USAGE);

    std::string cmd = argv[1];

    if (cmd == "stop") {
        if (argc != 2)
            throw std::invalid_argument("'stop' takes no arguments.");
        return {CommandType::Stop, std::nullopt};
    }

    if (cmd == "resume") {
        if (argc != 2)
            throw std::invalid_argument("'resume' takes no arguments.");
        return {CommandType::Resume, std::nullopt};
    }

    if (cmd == "scan") {
        if (argc < 3)
            throw std::invalid_argument(std::string("'scan' requires --path <path> or --all.\n") + USAGE);
        std::string flag = argv[2];
        if (flag == "--all") {
            if (argc != 3)
                throw std::invalid_argument("'scan --all' takes no further arguments.");
            return {CommandType::ScanAll, std::nullopt};
        }
        if (flag == "--path") {
            if (argc < 4)
                throw std::invalid_argument("'scan --path' requires a path argument.");
            if (argc > 4)
                throw std::invalid_argument("'scan --path' takes exactly one path argument.");
            return {CommandType::ScanPath, std::string(argv[3])};
        }
        throw std::invalid_argument(std::string("Unknown scan flag: ") + flag + "\n" + USAGE);
    }

    if (cmd == "signatures") {
        if (argc < 3)
            throw std::invalid_argument("'signatures' requires list, add, or remove.");
        std::string action = argv[2];
        if (action == "list") {
            if (argc != 3)
                throw std::invalid_argument("'signatures list' takes no arguments.");
            return {CommandType::SignatureList, std::nullopt};
        }
        if (action == "add") {
            if (argc < 4)
                throw std::invalid_argument("'signatures add' requires a value.");
            if (argc > 4)
                throw std::invalid_argument("'signatures add' takes exactly one value.");
            return {CommandType::SignatureAdd, std::string(argv[3])};
        }
        if (action == "remove") {
            if (argc < 4)
                throw std::invalid_argument("'signatures remove' requires an id.");
            if (argc > 4)
                throw std::invalid_argument("'signatures remove' takes exactly one id.");
            return {CommandType::SignatureRemove, std::string(argv[3])};
        }
        throw std::invalid_argument(std::string("Unknown signatures action: ") + action);
    }

    if (cmd == "exclusions") {
        if (argc < 3)
            throw std::invalid_argument("'exclusions' requires list, add, or remove.");
        std::string action = argv[2];
        if (action == "list") {
            if (argc != 3)
                throw std::invalid_argument("'exclusions list' takes no arguments.");
            return {CommandType::ExclusionList, std::nullopt};
        }
        if (action == "add") {
            if (argc < 4)
                throw std::invalid_argument("'exclusions add' requires a path.");
            if (argc > 4)
                throw std::invalid_argument("'exclusions add' takes exactly one path.");
            return {CommandType::ExclusionAdd, std::string(argv[3])};
        }
        if (action == "remove") {
            if (argc < 4)
                throw std::invalid_argument("'exclusions remove' requires an id.");
            if (argc > 4)
                throw std::invalid_argument("'exclusions remove' takes exactly one id.");
            return {CommandType::ExclusionRemove, std::string(argv[3])};
        }
        throw std::invalid_argument(std::string("Unknown exclusions action: ") + action);
    }

    if (cmd == "quarantine") {
        if (argc < 3)
            throw std::invalid_argument("'quarantine' requires list, restore, or delete.");
        std::string action = argv[2];
        if (action == "list") {
            if (argc != 3)
                throw std::invalid_argument("'quarantine list' takes no arguments.");
            return {CommandType::QuarantineList, std::nullopt};
        }
        if (action == "restore") {
            if (argc < 4)
                throw std::invalid_argument("'quarantine restore' requires an id.");
            if (argc > 4)
                throw std::invalid_argument("'quarantine restore' takes exactly one id.");
            return {CommandType::QuarantineRestore, std::string(argv[3])};
        }
        if (action == "delete") {
            if (argc < 4)
                throw std::invalid_argument("'quarantine delete' requires an id.");
            if (argc > 4)
                throw std::invalid_argument("'quarantine delete' takes exactly one id.");
            return {CommandType::QuarantineDelete, std::string(argv[3])};
        }
        throw std::invalid_argument(std::string("Unknown quarantine action: ") + action);
    }

    if (cmd == "config") {
        if (argc < 3)
            throw std::invalid_argument("'config' requires set-root.");
        std::string action = argv[2];
        if (action == "set-root") {
            if (argc < 4)
                throw std::invalid_argument("'config set-root' requires a path.");
            if (argc > 4)
                throw std::invalid_argument("'config set-root' takes exactly one path.");
            return {CommandType::ConfigSetRoot, std::string(argv[3])};
        }
        throw std::invalid_argument(std::string("Unknown config action: ") + action);
    }

    throw std::invalid_argument(std::string("Unknown command: ") + cmd + "\n" + USAGE);
}
