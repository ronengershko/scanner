#!/bin/bash
set -e

SCANNER_DIR="$(cd "$(dirname "$0")" && pwd)"

# Build
[ ! -d "$SCANNER_DIR/build" ] && cmake -B "$SCANNER_DIR/build" -S "$SCANNER_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SCANNER_DIR/build"

# Patch and install LaunchAgent (monitor — runs at login)
AGENT_SRC="$SCANNER_DIR/launchd/com.sentinel.scanner.agent.plist"
AGENT_DST=~/Library/LaunchAgents/com.sentinel.scanner.agent.plist
sed "s|__SCANNER_DIR__|$SCANNER_DIR|g" "$AGENT_SRC" > "$AGENT_DST"
launchctl unload "$AGENT_DST" 2>/dev/null || true
launchctl load "$AGENT_DST"
echo "LaunchAgent loaded (monitor on login)."

# Patch and install LaunchDaemon (scan --all — runs at boot, requires sudo)
DAEMON_SRC="$SCANNER_DIR/launchd/com.sentinel.scanner.daemon.plist"
DAEMON_DST=/Library/LaunchDaemons/com.sentinel.scanner.daemon.plist
echo "Installing LaunchDaemon requires sudo:"
sed "s|__SCANNER_DIR__|$SCANNER_DIR|g" "$DAEMON_SRC" | sudo tee "$DAEMON_DST" > /dev/null
sudo launchctl unload "$DAEMON_DST" 2>/dev/null || true
sudo launchctl load "$DAEMON_DST"
echo "LaunchDaemon loaded (scan --all on boot)."

echo ""
echo "Setup complete. Run './build/scanner --help' to get started."
