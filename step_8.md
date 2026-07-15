# Step 8 — File Cache

## What Was Done

Added cache hit/miss validation logic to `CacheRepository` and wrote persistence tests.

### Files Modified / Created

```
include/database/CacheRepository.h  (added isValidHit)
src/database/CacheRepository.cpp    (implemented isValidHit)
tests/test_cache.cpp                (new)
CMakeLists.txt                      (added test_cache target)
```

---

## Cache Hit Rules

A cached verdict is reused only when **all** of these match the current file state:

| Field | Why |
|---|---|
| device ID | same physical device |
| inode | same filesystem entry |
| size | file content unchanged |
| modification time | file content unchanged |
| signature version | signatures haven't changed |
| verdict != "error" | failed scans are never reused |

`CacheRepository::isValidHit(record, metadata, signatureVersion)` is a static method — it takes no DB, just compares values.

---

## How to Test

```bash
cmake --build build
./build/test_cache
```

Expected output: **19 passed, 0 failed.**
