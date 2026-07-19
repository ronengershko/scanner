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
    ConfigSetRoot,
    WatchList,
    WatchAdd,
    WatchRemove,
    Monitor
};

struct Command {
    CommandType type;
    std::optional<std::string> argument;
};
