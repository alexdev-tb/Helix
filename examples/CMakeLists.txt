add_subdirectory(hello_module)
add_subdirectory(modern_hello)

# Aggregate target to build all example .helx modules
add_custom_target(examples_helx)
if (TARGET hello_module_helx)
	add_dependencies(examples_helx hello_module_helx)
endif()
if (TARGET modern_hello_helx)
	add_dependencies(examples_helx modern_hello_helx)
endif()