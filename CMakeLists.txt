cmake_minimum_required(VERSION 3.16)
project(scanner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(scanner
    src/main.cpp
    src/app/AppConfig.cpp
    src/app/Application.cpp
    src/cli/CommandLineParser.cpp
    src/logging/Logger.cpp
    src/database/Database.cpp
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

target_link_libraries(scanner sqlite3)

add_executable(test_traverser
    tests/test_traverser.cpp
    src/scan/FileTraverser.cpp
    src/scan/FileMetadataProvider.cpp
)

add_executable(test_cache
    tests/test_cache.cpp
    src/database/Database.cpp
    src/database/CacheRepository.cpp
    src/scan/FileMetadataProvider.cpp
)
target_link_libraries(test_cache sqlite3)
