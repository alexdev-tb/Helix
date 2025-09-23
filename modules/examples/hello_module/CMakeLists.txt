cmake_minimum_required(VERSION 3.15)
project(hello_module)

# Produce a .helx package using the built helxcompiler
set(MODULES_DIR ${CMAKE_BINARY_DIR}/modules)
set(HELXCOMPILER_EXE ${CMAKE_BINARY_DIR}/helxcompiler)

add_custom_target(hello_module_helx ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MODULES_DIR}
    COMMAND ${HELXCOMPILER_EXE} -v -o ${MODULES_DIR}/hello-module.helx ${CMAKE_CURRENT_SOURCE_DIR}
    BYPRODUCTS ${MODULES_DIR}/hello-module.helx
    COMMENT "Building hello-module.helx with helxcompiler"
)

if (TARGET helxcompiler)
    add_dependencies(hello_module_helx helxcompiler)
else()
    message(STATUS "helxcompiler target not found. Ensure BUILD_TOOLS=ON at the top level.")
endif()