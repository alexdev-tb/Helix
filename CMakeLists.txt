cmake_minimum_required(VERSION 3.16)

# Centralized version management
include(cmake/Version.cmake)
helix_setup_version()

project(Helix VERSION ${HELIX_VERSION} LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories (public + generated version header)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${HELIX_GENERATED_INCLUDE_DIR})

# Find required packages
find_package(Threads REQUIRED)

# Core library
set(CORE_SOURCES
    src/core/module_loader.cpp
    src/core/manifest.cpp
    src/core/dependency_resolver.cpp
)

add_library(helix-core STATIC ${CORE_SOURCES})
target_include_directories(helix-core PUBLIC ${CMAKE_SOURCE_DIR}/include ${HELIX_GENERATED_INCLUDE_DIR})
target_link_libraries(helix-core ${CMAKE_DL_LIBS} Threads::Threads)

# Expose version to code
target_compile_definitions(helix-core PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")

# Daemon library
set(DAEMON_SOURCES
    src/daemon/daemon.cpp
    src/daemon/ipc_server.cpp
)

add_library(helix-daemon STATIC ${DAEMON_SOURCES})
target_include_directories(helix-daemon PUBLIC ${CMAKE_SOURCE_DIR}/include ${HELIX_GENERATED_INCLUDE_DIR})
target_link_libraries(helix-daemon helix-core)
target_compile_definitions(helix-daemon PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")

# Daemon executable
add_executable(helixd src/daemon/main.cpp)
target_link_libraries(helixd helix-daemon helix-core)
target_compile_definitions(helixd PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")
target_include_directories(helixd PUBLIC ${HELIX_GENERATED_INCLUDE_DIR})

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
option(BUILD_EXAMPLE_MODULE "Build example modules for testing" ON)
option(BUILD_TOOLS "Build helxcompiler and other tools" ON)

# Build tools first so helxcompiler target exists for examples
if(BUILD_TOOLS)
    add_subdirectory(tools/helxcompiler)
endif()

if(BUILD_EXAMPLE_MODULE)
    add_subdirectory(examples)
endif()

# Simple control client
add_executable(helixctl src/daemon/helixctl.cpp)
target_compile_definitions(helixctl PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")
target_include_directories(helixctl PUBLIC ${HELIX_GENERATED_INCLUDE_DIR})
install(TARGETS helixctl DESTINATION bin)