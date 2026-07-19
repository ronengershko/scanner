cmake_minimum_required(VERSION 3.16)
project(scanner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# ── Google Test ────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Scanner library (all sources except main.cpp) ─────────────────────────────
add_library(scanner_lib
    src/app/AppConfig.cpp
    src/app/Application.cpp
    src/cli/CommandLineParser.cpp
    src/logging/Logger.cpp
    src/database/Database.cpp
    src/database/RepositoryUtils.cpp
    src/database/SignatureRepository.cpp
    src/database/ScanSessionRepository.cpp
    src/database/CacheRepository.cpp
    src/database/ExclusionRepository.cpp
    src/database/QuarantineRepository.cpp
    src/scan/FileScanner.cpp
    src/scan/FileTraverser.cpp
    src/scan/FileMetadataProvider.cpp
    src/signatures/SignatureService.cpp
    src/scan/ScanService.cpp
    src/quarantine/QuarantineService.cpp
    src/exclusions/ExclusionService.cpp
)
target_link_libraries(scanner_lib sqlite3)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(scanner src/main.cpp)
target_link_libraries(scanner scanner_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
foreach(t cli scanner db integration)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} scanner_lib GTest::gtest_main)
endforeach()
