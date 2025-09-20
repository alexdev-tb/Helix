add_subdirectory(logger_module)

# Aggregate target to build all default .helx modules
add_custom_target(default_helx)
if (TARGET logger_module_helx)
    add_dependencies(default_helx logger_module_helx)
endif()
