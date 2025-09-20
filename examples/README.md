# Examples

This folder contains example Helix modules:

- `hello_module/` — Classic minimal module using default entry point symbols
  - Exports: helix_module_init/start/stop/destroy
  - Manifest does not specify `entry_points` (defaults apply)
- `modern_hello/` — Uses the Module Development Kit macros and custom entry point names
  - Implements functions with custom symbols via `HELIX_MODULE_*_AS()`
  - Manifest declares `entry_points` to match those symbols

To build these examples as shared libraries, enable examples in the top-level CMake with:

-DBUILD_EXAMPLE_MODULE=ON

Then build the project and use `helxcompiler` to package them into `.helx`, or install the built artifacts into the daemon's modules directory for testing.
