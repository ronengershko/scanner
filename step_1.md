# Step 1 — CLI Types, Parser, and Application

## What Was Done

Created the full project entry structure: CMake build, CLI types, parser, and application dispatcher.

### Files Created

```
CMakeLists.txt
include/cli/Command.h
include/cli/CommandLineParser.h
include/app/Application.h
src/cli/CommandLineParser.cpp
src/app/Application.cpp
src/main.cpp
```

### Design

- **`Command.h`** — defines `CommandType` (13 values) and `Command { type, optional<string> argument }`.
- **`CommandLineParser`** — parses raw `argc/argv` into a typed `Command`. Throws `std::invalid_argument` on unknown commands, missing arguments, or extra arguments.
- **`Application`** — catches parse errors (prints to stderr, returns 1), then dispatches each `CommandType` to a placeholder that prints what was received.
- **`main.cpp`** — constructs `Application`, calls `run`, returns the exit code. Nothing else.

No scan logic, no SQL, no business logic anywhere in this layer.

---

## How to Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binary is at `./build/scanner`.

---

## How to Run

```bash
# Scan commands
./build/scanner scan --path /some/path
./build/scanner scan --path "/path with spaces"
./build/scanner scan --all

# Control
./build/scanner stop
./build/scanner resume

# Signatures
./build/scanner signatures list
./build/scanner signatures add "evil_payload"
./build/scanner signatures remove 1

# Exclusions
./build/scanner exclusions list
./build/scanner exclusions add /some/path
./build/scanner exclusions remove 1

# Quarantine
./build/scanner quarantine list
./build/scanner quarantine restore <id>
./build/scanner quarantine delete <id>
```

---

## How to Test

### Valid commands — all should exit 0 and print a confirmation line

```bash
BIN=./build/scanner
$BIN scan --path /tmp/test
$BIN scan --path "/Users/ronen/Test Files"
$BIN scan --all
$BIN stop
$BIN resume
$BIN signatures list
$BIN signatures add "evil_payload"
$BIN signatures remove 42
$BIN exclusions list
$BIN exclusions add /tmp/excluded
$BIN exclusions remove 7
$BIN quarantine list
$BIN quarantine restore abc-123
$BIN quarantine delete abc-123
```

### Invalid commands — all should exit 1 with a clear error

```bash
$BIN                        # no command
$BIN unknown                # unknown command
$BIN scan                   # missing --path or --all
$BIN scan --path            # missing path argument
$BIN scan --all extra       # extra argument rejected
$BIN stop extra             # extra argument rejected
$BIN signatures             # missing action
$BIN signatures add         # missing value
$BIN quarantine delete      # missing id
```

### Verify exit codes

```bash
./build/scanner scan --path /tmp/test; echo "exit: $?"   # expect 0
./build/scanner unknown; echo "exit: $?"                 # expect 1
```
