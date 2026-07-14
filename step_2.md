# Step 2 — Local Configuration and Data Paths

## What Was Done

Added `AppConfig` to centralize all runtime paths. Updated `Application` and `main.cpp` to load and use the config.

### Files Created / Modified

```
include/app/AppConfig.h        (new)
src/app/AppConfig.cpp          (new)
include/app/Application.h      (updated — constructor now takes AppConfig)
src/app/Application.cpp        (updated — constructor creates directories)
src/main.cpp                   (updated — loads config, catches fatal errors)
CMakeLists.txt                 (updated — added AppConfig.cpp)
```

### Design

- `AppConfig::load()` resolves the project root from the executable's location (`build/scanner` → parent = project root), then builds all paths relative to it. No hardcoded strings anywhere else.
- `createRequiredDirectories()` creates `scanner-data/` and `scanner-data/quarantine/` with a single `create_directories` call (parents are created automatically). Idempotent.
- `Application` constructor calls `createRequiredDirectories()` — directories exist before any command runs.
- `main.cpp` wraps construction in a try/catch to handle fatal config/IO failures with a clean message.

### Runtime layout produced

```
scanner-data/
└── quarantine/
```

`scanner.db` and `scanner.log` will be created by later steps inside `scanner-data/`.

---

## How to Build

```bash
./build.sh
```

---

## How to Test

### Directories created on first run

```bash
rm -rf scanner-data
./build/scanner stop
find scanner-data -type d
# Expected:
# scanner-data
# scanner-data/quarantine
```

### Idempotent — second run does not fail

```bash
./build/scanner stop
echo $?   # expect 0
```

### All commands still work normally

```bash
./build/scanner scan --path /tmp/test
./build/scanner signatures add "evil_payload"
./build/scanner quarantine list
```
