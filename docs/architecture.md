# Helix Architecture Overview

Helix follows a microkernel-style design: a small core with pluggable modules loaded at runtime.

## Components

- Core libraries
  - ModuleLoader: dlopen/dlsym wrapper, lifecycle calls (init/start/stop/destroy)
  - Manifest: parse module `manifest.json`
  - DependencyResolver: dependency graph and load order
- Daemon (`helixd`)
  - Manages installed modules in a directory
  - Exposes a simple line-oriented IPC server over a Unix socket (default /tmp/helixd.sock)
  - Commands: install/enable/start/stop/disable/uninstall/list/status/info/version
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

## Concurrency and threading model

Helix’s core and daemon execute module lifecycle calls synchronously:

- `init`, `start`, `stop`, and `destroy` are invoked inline on the daemon’s control thread.
- The IPC control loop is single-threaded; while a lifecycle call runs, other control requests wait.

Implications:

- Modules must keep lifecycle functions short and non-blocking. Perform long-running work in your own threads and return promptly from `start()`.
- If a module’s `start()` blocks, it prevents other modules from being started or stopped and stalls IPC responses until it returns.
- On `stop()`, signal worker threads and join with a short timeout to avoid hanging shutdown.

Recommended pattern inside a module:

1. `init()` — allocate resources, initialize state; return quickly.
2. `start()` — spawn worker thread(s) or event loop and immediately return 0.
3. `stop()` — signal shutdown, join threads; bound the wait time.
4. `destroy()` — free resources.

Logging is decoupled via a registry: `helix_log()` queues messages until at least one Logger module registers a sink, then dispatches to all registered sinks via a `helix_log_dispatch` symbol exported by the daemon/logging integration. See `include/helix/log.h` for details.

## Future work

- Event bus and subscriptions
- Capability/permission system
