cmake_minimum_required(VERSION 3.16)
project(Helix VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find required packages
find_package(Threads REQUIRED)

# Core library
set(CORE_SOURCES
    src/core/module_loader.cpp
    src/core/manifest.cpp
    src/core/dependency_resolver.cpp
)

add_library(helix-core STATIC ${CORE_SOURCES})
target_include_directories(helix-core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(helix-core ${CMAKE_DL_LIBS} Threads::Threads)

# Daemon library
set(DAEMON_SOURCES
    src/daemon/daemon.cpp
)

add_library(helix-daemon STATIC ${DAEMON_SOURCES})
target_include_directories(helix-daemon PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(helix-daemon helix-core)

# Daemon executable
add_executable(helixd src/daemon/main.cpp)
target_link_libraries(helixd helix-daemon helix-core)

# Installation
install(TARGETS helixd DESTINATION bin)
install(DIRECTORY include/helix DESTINATION include)
install(TARGETS helix-core helix-daemon DESTINATION lib)

# Create modules directory in build
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/modules)

# Print build information
message(STATUS "Helix Framework ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# Example module for testing (optional)
option(BUILD_EXAMPLE_MODULE "Build example module for testing" OFF)
option(BUILD_TOOLS "Build helxcompiler and other tools" ON)

if(BUILD_EXAMPLE_MODULE)
    add_subdirectory(examples)
endif()

if(BUILD_TOOLS)
    add_subdirectory(tools/helxcompiler)
endif()