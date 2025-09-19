# Helix Architecture Overview

Helix follows a microkernel-style design: a small core with pluggable modules loaded at runtime.

## Components

- Core libraries
  - ModuleLoader: dlopen/dlsym wrapper, lifecycle calls (init/start/stop/destroy)
  - Manifest: parse module `manifest.json`
  - DependencyResolver: dependency graph and load order
- Daemon (`helixd`)
  - Manages installed modules in a directory
  - Commands: install/enable/start/stop/disable/uninstall/list/status
- Module compiler (`helxcompiler`)
  - Compiles sources to a shared library and creates `.helx` (tar.gz)
  - Auto-detects name/version from `HELIX_MODULE_DECLARE`

## Module package (.helx)

Tar.gz with two top-level entries:

- `manifest.json` (name, version, entry_points, binary_path, dependencies)
- `lib<name>.so` (module binary)

## Lifecycle

1. install → copies/expands package into modules dir/name
2. enable → load `.so`, resolve symbols, call init
3. start → call start
4. stop → call stop
5. disable → call destroy and unload
6. uninstall → remove files if no dependents

## Future work

- Event bus and subscriptions
- Capability/permission system
- Remote control (CLI/IPC)
- Hot reload
