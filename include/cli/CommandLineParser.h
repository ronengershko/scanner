#pragma once
#include "Command.h"

class CommandLineParser {
public:
    Command parse(int argc, char* argv[]) const;
};
