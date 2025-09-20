cmake_minimum_required(VERSION 3.15)
project(modern_hello)



# Produce a .helx package using the built helxcompiler
set(MODULES_DIR ${CMAKE_BINARY_DIR}/modules)
set(HELXCOMPILER_EXE ${CMAKE_BINARY_DIR}/helxcompiler)

add_custom_target(modern_hello_helx ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MODULES_DIR}
    COMMAND ${HELXCOMPILER_EXE} -v -n modern-hello
            --ep-init my_init --ep-start my_start --ep-stop my_stop --ep-destroy my_destroy
            -o ${MODULES_DIR}/modern-hello.helx ${CMAKE_CURRENT_SOURCE_DIR}
    BYPRODUCTS ${MODULES_DIR}/modern-hello.helx
    COMMENT "Building modern-hello.helx with helxcompiler"
)

# Ensure helxcompiler is built before packaging
if (TARGET helxcompiler)
    add_dependencies(modern_hello_helx helxcompiler)
else()
    message(STATUS "helxcompiler target not found. Ensure BUILD_TOOLS=ON at the top level.")
endif()