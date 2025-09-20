# Helix API Index

This directory indexes public APIs and points to headers in `include/helix/`.

## Module Development Kit (MDK)

- `include/helix/module.h`
  - Recommended macros (short aliases): `HELIX_INIT(name)`, `HELIX_START(name)`, `HELIX_STOP(name)`, `HELIX_DISABLE(name)`.
  - Optional: `HELIX_MODULE_DECLARE` to expose runtime accessors (`helix_module_get_name`, `helix_module_get_version`, ...). Most modules can omit this since metadata comes from `manifest.json`.
  - Context struct: `helix::ModuleContext`

Entry points that modules must export (default symbols):

- `int helix_module_init()`
- `int helix_module_start()`
- `int helix_module_stop()`
- `void helix_module_destroy()`

These symbol names can be customized per-module by declaring an `entry_points` map in the module's `manifest.json`:

```
"entry_points": {
  "init": "my_init",
  "start": "my_start",
  "stop": "my_stop",
  "destroy": "my_destroy"
}
```

Use the MDK macros `HELIX_MODULE_*_AS(symbol)` to define functions with custom names.

## Daemon and Core

- `include/helix/daemon.h` — daemon management API (install/enable/start/stop...)
- `include/helix/module_loader.h` — dynamic loading and lifecycle calls
- `include/helix/manifest.h` — manifest parsing and data model
- `include/helix/dependency_resolver.h` — dependency resolution interfaces

Refer to `src/` for implementation details.
