# Build the helxcompiler tool
add_executable(helxcompiler 
    main.cpp
    compiler.cpp
)

# Include directories
target_include_directories(helxcompiler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${HELIX_GENERATED_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries (we'll need libarchive for .helx packaging)
# For now, we'll use system tar command as a fallback
target_link_libraries(helxcompiler helix-core ${CMAKE_DL_LIBS} Threads::Threads)

# Propagate optional JSON library usage
if(nlohmann_json_FOUND)
    target_link_libraries(helxcompiler PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(helxcompiler PUBLIC HELIX_USE_NLOHMANN_JSON=1)
endif()

# Set output directory
set_target_properties(helxcompiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Installation
install(TARGETS helxcompiler DESTINATION bin)