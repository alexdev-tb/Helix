# Helix API Index

This directory indexes public APIs and points to headers in `include/helix/`.

## Module Development Kit (MDK)

- `include/helix/module.h`
  - Macros to declare entry points: `HELIX_MODULE_INIT[_AS]`, `HELIX_MODULE_START[_AS]`, `HELIX_MODULE_STOP[_AS]`, `HELIX_MODULE_DESTROY[_AS]`.
  - Optional: `HELIX_MODULE_DECLARE(name, version, description, author)` to expose runtime accessors. Most modules can omit this since metadata comes from `manifest.json`.
  - Context helper: `HELIX_MODULE_CONTEXT()` yields a `helix::ModuleContext` with basic info.

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

## Logging API

- Header: `include/helix/log.h`
- For modules: call `helix_log(const char* module_name, const char* message, HelixLogLevel level)`.
  - If no logger is present yet, messages are queued in a bounded buffer (256 msgs) and flushed later.
  - Once the dispatcher symbol `helix_log_dispatch` becomes available (exported by the daemon/log registry), messages are dispatched to all registered sinks.
- For logger modules: register and unregister your sink function at `start()`/`stop()` time:

```cpp
using LogSink = void(*)(const char*, int, const char*);
static void my_sink(const char* mod, int lvl, const char* msg) { /* write somewhere */ }

if (auto reg = helix_log_get_register()) reg(&my_sink);
// ... on stop
if (auto unreg = helix_log_get_unregister()) unreg(&my_sink);
```

Notes:

- Levels: `HELIX_LOG_DEBUG`, `HELIX_LOG_INFO`, `HELIX_LOG_WARN`, `HELIX_LOG_ERROR`.
- Threading: `helix_log()` uses a mutex-protected queue and dlsym lookups; it’s safe to call from module worker threads. Sinks should do minimal work or hand off asynchronously to avoid stalling producers.
