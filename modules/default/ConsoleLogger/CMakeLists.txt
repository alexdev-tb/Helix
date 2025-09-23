cmake_minimum_required(VERSION 3.15)
project(consolelogger)

set(MODULES_DIR ${CMAKE_BINARY_DIR}/modules)
set(HELXCOMPILER_EXE ${CMAKE_BINARY_DIR}/helxcompiler)

add_custom_target(consolelogger_helx ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MODULES_DIR}
    COMMAND ${HELXCOMPILER_EXE} -v -o ${MODULES_DIR}/consolelogger.helx ${CMAKE_CURRENT_SOURCE_DIR}
    BYPRODUCTS ${MODULES_DIR}/consolelogger.helx
    COMMENT "Building consolelogger.helx with helxcompiler"
)

if (TARGET helxcompiler)
    add_dependencies(consolelogger_helx helxcompiler)
else()
    message(STATUS "helxcompiler target not found. Ensure BUILD_TOOLS=ON at the top level.")
endif()
