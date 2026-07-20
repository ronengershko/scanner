# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

macOS malware scanner (C++17) — a SentinelOne home assignment. The full design is in `PLAN.md`. No source code exists yet; implementation follows the 17-step plan there.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build
cmake --build build

# Run
./build/scanner <command>
```

Tests framework is TBD — will be added in step 17.

## CLI Commands

```
scanner scan --path <path>
scanner scan --all
scanner stop
scanner resume
scanner signatures list|add <hex>|remove <id>
scanner exclusions list|add <path>|remove <id>
scanner quarantine list|restore <id>|delete <id>
```

## Architecture

Layered C++ design — no business logic in `main.cpp`, no SQL in service classes, no direct DB access from app/cli layers.

**Layers (top to bottom):**

1. **CLI** (`cli/`) — `CommandLineParser` produces a `Command` struct; `Application` dispatches it
2. **Services** (`scan/`, `signatures/`, `exclusions/`, `quarantine/`, `logging/`) — business logic only, call repositories for persistence
3. **Repositories** (`database/`) — all SQL lives here; one repository per domain entity
4. **Platform** — `FileMetadataProvider` wraps `stat()`, `FileTraverser` walks directories

**Key design decisions from PLAN.md:**
- File identity = `(device_id, inode)` — stable across renames
- Cache invalidation key = `(device_id, inode, path, size, mtime, signature_version)`
- Directory traversal is alphabetically sorted for deterministic, resumable ordering
- Stop/resume uses a checkpoint (last completed file path) in `scan_sessions` — no in-memory pending list
- SQLite for all persistence (`scanner-data/scanner.db`)
- macOS LaunchAgent (`com.sentinel.scanner.plist`) for auto-start on login

**Runtime data layout:**
```
scanner-data/
├── scanner.db
├── scanner.log
└── quarantine/
```

## Database Schema (7 tables)

`signatures`, `scanner_metadata`, `file_cache`, `scan_sessions`, `exclusions`, `quarantine_items` — see PLAN.md §5 for full SQL.

## File Scanning Algorithm

Read in binary chunks → search all signatures with chunk-boundary overlap → return first match and stop. Binary-safe (handles zero bytes).

## Implementation Order

Follow PLAN.md steps 1–17 in order. Each step has explicit completion criteria. Steps 1–9 cover core scanning; 10–13 add stop/resume, quarantine, exclusions; 14–17 finish CLI output, full-scan mode, LaunchAgent, and tests.
