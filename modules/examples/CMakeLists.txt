add_subdirectory(hello_module)

# Aggregate target to build all example .helx modules
add_custom_target(examples_helx)
if (TARGET hello_module_helx)
	add_dependencies(examples_helx hello_module_helx)
endif()
