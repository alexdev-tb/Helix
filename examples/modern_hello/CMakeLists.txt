cmake_minimum_required(VERSION 3.15)
project(modern_hello)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Include Helix headers
include_directories(${CMAKE_SOURCE_DIR}/../../include)

# Create the module shared library
add_library(modern_hello SHARED modern_hello.cpp)

# Link required libraries
target_link_libraries(modern_hello 
    PRIVATE 
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# Set shared library properties
set_target_properties(modern_hello PROPERTIES
    PREFIX ""  # Remove lib prefix
    SUFFIX ".so"
    VERSION 2.0.0
    SOVERSION 2
)

# Install the module
install(TARGETS modern_hello
    LIBRARY DESTINATION lib/helix/modules
    RUNTIME DESTINATION lib/helix/modules
)

# Install the manifest
install(FILES manifest.json 
    DESTINATION lib/helix/modules
    RENAME modern_hello.manifest.json
)