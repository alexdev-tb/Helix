# Helix API Index

This directory indexes public APIs and points to headers in `include/helix/`.

## Module Development Kit (MDK)

- `include/helix/module.h`
  - Macros: `HELIX_MODULE_DECLARE`, `HELIX_MODULE_INIT`, `HELIX_MODULE_START`,
    `HELIX_MODULE_STOP`, `HELIX_MODULE_DESTROY`
  - Accessors: `helix_module_get_name`, `helix_module_get_version`, etc.
  - Context struct: `helix::ModuleContext`

Entry points that modules must export:

- `int helix_module_init()`
- `int helix_module_start()`
- `int helix_module_stop()`
- `void helix_module_destroy()`

## Daemon and Core

- `include/helix/daemon.h` — daemon management API (install/enable/start/stop...)
- `include/helix/module_loader.h` — dynamic loading and lifecycle calls
- `include/helix/manifest.h` — manifest parsing and data model
- `include/helix/dependency_resolver.h` — dependency resolution interfaces

Refer to `src/` for implementation details.
