cmake_minimum_required(VERSION 3.16)

include(cmake/Version.cmake)
helix_setup_version()

project(Helix VERSION ${HELIX_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${HELIX_GENERATED_INCLUDE_DIR})

find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 QUIET)

set(CORE_SOURCES
    src/core/module_loader.cpp
    src/core/manifest.cpp
    src/core/dependency_resolver.cpp
)

add_library(helix-core STATIC ${CORE_SOURCES})
target_include_directories(helix-core PUBLIC ${CMAKE_SOURCE_DIR}/include ${HELIX_GENERATED_INCLUDE_DIR})
target_link_libraries(helix-core ${CMAKE_DL_LIBS} Threads::Threads)
if(nlohmann_json_FOUND)
    target_link_libraries(helix-core PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(helix-core PUBLIC HELIX_USE_NLOHMANN_JSON=1)
    message(STATUS "Using nlohmann_json for JSON parsing")
else()
    message(STATUS "nlohmann_json not found; falling back to simplified/regex JSON parsing")
endif()

target_compile_definitions(helix-core PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")

set(DAEMON_SOURCES
    src/daemon/daemon.cpp
    src/daemon/ipc_server.cpp
)

add_library(helix-daemon STATIC ${DAEMON_SOURCES})
target_include_directories(helix-daemon PUBLIC ${CMAKE_SOURCE_DIR}/include ${HELIX_GENERATED_INCLUDE_DIR})
target_link_libraries(helix-daemon helix-core)
target_compile_definitions(helix-daemon PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")

add_executable(helixd src/daemon/main.cpp)
target_link_libraries(helixd helix-daemon helix-core)
target_compile_definitions(helixd PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")
target_include_directories(helixd PUBLIC ${HELIX_GENERATED_INCLUDE_DIR})
target_sources(helixd PRIVATE src/daemon/log_registry.cpp)
if (UNIX)
    target_link_options(helixd PRIVATE -Wl,--export-dynamic)
endif()

install(TARGETS helixd DESTINATION bin)
install(DIRECTORY include/helix DESTINATION include)
install(TARGETS helix-core helix-daemon DESTINATION lib)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/modules)

message(STATUS "Helix Framework ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

option(BUILD_EXAMPLE_MODULE "Build example modules for testing" ON)
option(BUILD_DEFAULT_MODULE "Build default modules" ON)
option(BUILD_TOOLS "Build helxcompiler and other tools" ON)

if(BUILD_TOOLS)
    add_subdirectory(tools/helxcompiler)
endif()

if(BUILD_EXAMPLE_MODULE)
    add_subdirectory(modules/examples)
endif()

if(BUILD_DEFAULT_MODULE)
    add_subdirectory(modules/default)
endif()

add_executable(helixctl src/daemon/helixctl.cpp)
target_compile_definitions(helixctl PUBLIC HELIX_CORE_VERSION="${PROJECT_VERSION}")
target_include_directories(helixctl PUBLIC ${HELIX_GENERATED_INCLUDE_DIR})
install(TARGETS helixctl DESTINATION bin)