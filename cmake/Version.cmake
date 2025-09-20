# Centralized version management for Helix
#
# Usage (top-level CMakeLists.txt):
#   include(cmake/Version.cmake)
#   helix_setup_version()
#
# This provides variables and a header for use in C++ code:
#   HELIX_VERSION_MAJOR/MINOR/PATCH
#   HELIX_VERSION (full string)
#   HELIX_GIT_DESC (optional, from `git describe` if available)
# It also optionally generates include/helix/version.h during configure.

function(helix_setup_version)
  # Allow override via -DHELIX_VERSION=MAJOR.MINOR.PATCH
  if(NOT HELIX_VERSION)
    set(HELIX_VERSION 2.0.0 CACHE STRING "Helix version")
  endif()

  # Independent API version (semantic): allows compatibility tracking separate from core release
  if(NOT HELIX_API_VERSION)
    set(HELIX_API_VERSION 1.0.0 CACHE STRING "Helix API version")
  endif()

  # Parse parts
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${HELIX_VERSION})
  if(NOT CMAKE_MATCH_0)
    message(FATAL_ERROR "HELIX_VERSION '${HELIX_VERSION}' must be MAJOR.MINOR.PATCH")
  endif()
  set(HELIX_VERSION_MAJOR ${CMAKE_MATCH_1} PARENT_SCOPE)
  set(HELIX_VERSION_MINOR ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(HELIX_VERSION_PATCH ${CMAKE_MATCH_3} PARENT_SCOPE)
  set(HELIX_VERSION ${HELIX_VERSION} PARENT_SCOPE)
  set(HELIX_API_VERSION ${HELIX_API_VERSION} PARENT_SCOPE)

  # Try to capture Git describe (best effort)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE _GIT_DESC
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(_GIT_DESC)
      set(HELIX_GIT_DESC ${_GIT_DESC} PARENT_SCOPE)
    endif()
  endif()

  # Configure a version header (generated at build tree)
  set(_VER_HDR ${CMAKE_BINARY_DIR}/generated/helix/version.h)
  configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${_VER_HDR} @ONLY)
  # Expose generated include dir
  set(HELIX_GENERATED_INCLUDE_DIR ${CMAKE_BINARY_DIR}/generated PARENT_SCOPE)
endfunction()
