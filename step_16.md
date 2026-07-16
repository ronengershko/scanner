# Step 16 — macOS Startup Integration

## What Was Done

Created two plist files:
- `launchd/com.sentinel.scanner.agent.plist` — LaunchAgent (runs at user login, as your user)
- `launchd/com.sentinel.scanner.daemon.plist` — LaunchDaemon (runs at OS boot, as root)

The scan root is configured in the DB via `scanner config set-root`, so neither plist touches
what gets scanned — that's fully under your control before loading either service.

---

## Key Difference

| | LaunchAgent | LaunchDaemon |
|---|---|---|
| Starts at | User login | OS boot |
| Runs as | Your user | root |
| Install location | `~/Library/LaunchAgents/` | `/Library/LaunchDaemons/` |
| Install requires | No sudo | sudo |
| File access | Your files only | Entire disk |

---

## Setup (do this once before both tests)

```bash
# Add malicious signature
./build/scanner signatures add "bad_virus"

# Create test folders
mkdir -p /tmp/test-agent /tmp/test-daemon

# Agent test files
echo "hello world" > /tmp/test-agent/file1.txt
echo "clean file"  > /tmp/test-agent/file2.txt
echo "bad_virus"   > /tmp/test-agent/malicious.txt

# Daemon test files
echo "hello world" > /tmp/test-daemon/file1.txt
echo "clean file"  > /tmp/test-daemon/file2.txt
echo "bad_virus"   > /tmp/test-daemon/malicious.txt
```

---

## Test 1 — LaunchAgent

### Setup
```bash
# Point scanner at agent test folder
./build/scanner config set-root /tmp/test-agent
```

### Install
```bash
cp launchd/com.sentinel.scanner.agent.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.sentinel.scanner.agent.plist
```

### Verify it ran
```bash
# Check the log
tail -20 scanner-data/scanner.log

# Check a scan session was created
sqlite3 scanner-data/scanner.db \
  "SELECT id, canonical_path, status, scanned_files FROM scan_sessions ORDER BY id DESC LIMIT 3;"
```

### Uninstall
```bash
launchctl unload ~/Library/LaunchAgents/com.sentinel.scanner.agent.plist
rm ~/Library/LaunchAgents/com.sentinel.scanner.agent.plist
```

---

## Test 2 — LaunchDaemon

### Setup
```bash
# Point scanner at daemon test folder
./build/scanner config set-root /tmp/test-daemon
```

### Install (requires sudo)
```bash
sudo cp launchd/com.sentinel.scanner.daemon.plist /Library/LaunchDaemons/
sudo launchctl load /Library/LaunchDaemons/com.sentinel.scanner.daemon.plist
```

### Verify it ran
```bash
tail -20 scanner-data/scanner.log

sqlite3 scanner-data/scanner.db \
  "SELECT id, canonical_path, status, scanned_files FROM scan_sessions ORDER BY id DESC LIMIT 3;"
```

### Uninstall
```bash
sudo launchctl unload /Library/LaunchDaemons/com.sentinel.scanner.daemon.plist
sudo rm /Library/LaunchDaemons/com.sentinel.scanner.daemon.plist
```

---

## Notes

- `RunAtLoad=true` means the scan runs immediately on install — no need to log out/in to test.
- The scanner checks for a running session at startup (`findRunning`) — duplicate scans are rejected.
- Both plists write stdout and stderr to `scanner-data/scanner.log`.
- To test login behavior: install, log out, log back in — a new scan session appears in the DB.
