# Build the hello module as a shared library
add_library(hello-module SHARED hello_module.cpp)

# Set output directory to match manifest binary_path
set_target_properties(hello-module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT_NAME "hello-module"
)

# Copy manifest to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
    ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
    COPYONLY
)

# Create module package directory for easy testing
set(MODULE_PACKAGE_DIR ${CMAKE_BINARY_DIR}/modules/hello-module)
add_custom_command(TARGET hello-module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MODULE_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hello-module> ${MODULE_PACKAGE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/manifest.json ${MODULE_PACKAGE_DIR}/
    COMMENT "Creating hello-module package"
)