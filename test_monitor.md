# Monitor Manual Test

## Setup

```bash
# Build
cmake --build build

# Add a test signature
./build/scanner signatures add virus

# Create watched directories (inside home, not /tmp — /tmp is wiped on reboot)
mkdir -p ~/watch-test
mkdir -p ~/watch-test2

# Add watch path
./build/scanner watch add ~/watch-test

# Verify
./build/scanner watch list
```

## Start the monitor

The monitor starts automatically at login via LaunchAgent. To run it manually in terminal 1:
```bash
./build/scanner monitor
```

Expected output:
```
Monitoring: /Users/<you>/watch-test
(Ctrl+C to stop)
```

## Test 1 — Clean file (no alert)

In terminal 2:
```bash
echo "hello world" > ~/watch-test/clean.txt
```

Expected: no output in monitor terminal, nothing in log.

## Test 2 — Malicious file (detected + quarantined)

```bash
echo "virus payload" > ~/watch-test/evil.txt
```

Expected in monitor terminal:
```
MALICIOUS: /Users/<you>/watch-test/evil.txt
```

Verify the file was removed:
```bash
ls ~/watch-test/
# evil.txt should be gone

./build/scanner quarantine list
# evil.txt should appear as quarantined
```

## Test 3 — Modify an existing file

```bash
echo "virus" >> ~/watch-test/clean.txt
```

Expected: FSEvents fires on modification, file is scanned and quarantined.

## Test 4 — Add a new watch path while monitor is running

While monitor is still running in terminal 1, in terminal 2:
```bash
./build/scanner watch add ~/watch-test2
```

Expected in terminal 2:
```
Watch path added: /Users/<you>/watch-test2
Monitor reloaded.
```

Monitor restarts automatically and now watches both paths:
```
Monitoring: /Users/<you>/watch-test
Monitoring: /Users/<you>/watch-test2
(Ctrl+C to stop)
```

Drop a file in the new path:
```bash
echo "virus" > ~/watch-test2/evil2.txt
```

Expected: evil2.txt caught and quarantined.

## Test 5 — Remove a watch path while monitor is running

```bash
./build/scanner watch list           # note the id of watch-test2
./build/scanner watch remove <id>
```

Expected: "Monitor reloaded." — monitor restarts watching only the remaining path.

## Test 6 — Only one monitor runs at a time

Start a second monitor in terminal 2 while terminal 1 is already running one:
```bash
./build/scanner monitor
```

Expected: the first monitor is killed automatically, the new one takes over.

```bash
./build/scanner monitor sessions
# Should show previous session as stopped, new one as running
```

## Check session history

```bash
./build/scanner monitor sessions
# ID   PID    Status   Started              Stopped
# 2    ....   running  2026-...
# 1    ....   stopped  2026-...             2026-...
```

## Cleanup

```bash
./build/scanner watch list
./build/scanner watch remove <id>   # repeat for each id
./build/scanner watch list          # should be empty
rm -rf ~/watch-test ~/watch-test2
```
