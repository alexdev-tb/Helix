# Build the helxcompiler tool
add_executable(helxcompiler 
    main.cpp
    compiler.cpp
)

# Include directories
target_include_directories(helxcompiler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries (we'll need libarchive for .helx packaging)
# For now, we'll use system tar command as a fallback
target_link_libraries(helxcompiler ${CMAKE_DL_LIBS})

# Set output directory
set_target_properties(helxcompiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Installation
install(TARGETS helxcompiler DESTINATION bin)