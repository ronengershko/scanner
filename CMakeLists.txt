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
)
