#pragma once
#include <optional>
#include <string>

enum class CommandType {
    ScanPath,
    ScanAll,
    Stop,
    Resume,
    SignatureList,
    SignatureAdd,
    SignatureRemove,
    ExclusionList,
    ExclusionAdd,
    ExclusionRemove,
    QuarantineList,
    QuarantineRestore,
    QuarantineDelete,
    ConfigSetRoot
};

struct Command {
    CommandType type;
    std::optional<std::string> argument;
};
