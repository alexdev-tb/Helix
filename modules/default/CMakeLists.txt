add_subdirectory(ConsoleLogger)

# Aggregate target to build all default .helx modules
add_custom_target(default_helx)
if (TARGET ConsoleLogger_helx)
    add_dependencies(default_helx ConsoleLogger_helx)
endif()
