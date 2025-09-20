# Helix — Build and Usage Guide

This guide covers building Helix, compiling a module into a `.helx` package, and
managing modules with the daemon.

## Prerequisites

- Linux, bash
- CMake ≥ 3.16, a C++17 compiler (g++)
- tar (for `.helx` packaging/extraction)

Optional:

- `HELIX_ROOT` env var pointing to the repo root helps `helxcompiler` find headers

## Build the project

From the repo root, using CMake Presets (recommended):

```bash
cmake --preset debug
cmake --build --preset debug -j

# or Release
cmake --preset release
cmake --build --preset release -j
```

Artifacts appear under the chosen build dir: `build-debug/` or `build-release/`.

- `helixd` — the daemon
- `helxcompiler` — compiles modules and produces `.helx`
- Static libs: `libhelix-core.a`, `libhelix-daemon.a`

### Logging (multi-sink)

Helix doesn't print module messages from core. Modules call a tiny API and one or more Logger modules receive and handle logs.

- In code, include `helix/log.h` and call:

  - `helix_log("MyModule", "Initializing...", HELIX_LOG_INFO);`

- The call is a no-op until a Logger module is enabled and started. Messages are queued (up to 256) and flushed once logging is available.
- Multiple Logger modules can register concurrently; all receive every log message.

Quick start with the example logger:

```bash
# From the build directory
./helixd --modules-dir ./modules --foreground &
./helixctl install modules/logger-module.helx
./helixctl enable Logger-module
./helixctl start Logger-module

# Now install/enable/start your other modules; their helix_log() calls will appear
./helixctl install modules/hello-module.helx
./helixctl enable hello-module
./helixctl start hello-module
```

Implementing a Logger module:

- Logger modules register a sink function when they start and can unregister it when they stop:

```c++
using LogSink = void(*)(const char* module, int level, const char* msg);

static void my_sink(const char* module, int level, const char* msg) { /* format + write */ }

if (auto reg = helix_log_get_register()) reg(&my_sink);
// ... later on stop
if (auto unreg = helix_log_get_unregister()) unreg(&my_sink);
```

Levels available: `HELIX_LOG_DEBUG`, `HELIX_LOG_INFO`, `HELIX_LOG_WARN`, `HELIX_LOG_ERROR`.

### Keep lifecycle non-blocking

The daemon calls `init/start/stop/destroy` synchronously on its control thread. Keep these functions short and non-blocking: start worker threads in `start()` and return immediately; on `stop()`, signal and join with a bounded wait. This ensures other modules and IPC commands remain responsive.

## Compile a module to .helx

From within the build dir:

```bash
./helxcompiler -v -o hello-module.helx ../modules/examples/hello_module/
```

`helxcompiler` options:

- `-o, --output <file>`: output `.helx` (default: <module_name>.helx)
- `-n, --name <name>`: override module name
- `-I, --include <path>`: add include directory (auto-detects Helix include)
- `-L, --library-path <path>`: add library search path
- `-l, --library <lib>`: link against library
- `--std <standard>`: C++ standard (default: c++17)
- `-O, --optimize <level>`: optimization level (default: -O2)
- `-g, --debug`: include debug info
- `-v, --verbose`: verbose output

Notes:

- Metadata (name, version, description, author, capabilities) is sourced from `manifest.json`.
- You no longer need to add `HELIX_MODULE_DECLARE` in code. It's optional and only useful if you want runtime accessors like `helix_module_get_name()`.
- The `.helx` tarball will contain `manifest.json` and `lib<name>.so` at the top level.
- Version fields defaults: if `manifest.json` omits `minimum_api_version` or `minimum_core_version`, the compiler fills them in with the current framework versions — `minimum_api_version` defaults to `HELIX_API_VERSION` and `minimum_core_version` defaults to `HELIX_VERSION` (both come from the generated header `helix/version.h` and can be set at configure time via `-DHELIX_API_VERSION=` and `-DHELIX_VERSION=`).

## Run the daemon

From the build dir:

```bash
mkdir -p modules
./helixd ./modules
```

While running in interactive mode, type commands at the prompt:

- `status` — show daemon status
- `list` — list installed modules
- `install <file.helx>` — install from a `.helx` package only
- `enable <name>` — load and initialize
- `start <name>` — start a module
- `stop <name>` — stop a running module
- `disable <name>` — unload (calls destroy)
- `uninstall <name>` — remove files and unregister
- `help` — show commands
- `exit` — shutdown daemon

The CLI shows a simple `helix>` prompt with colored status messages.

Daemon options:

- `-h, --help` — show usage and exit
- `--version` — print version and exit
- `--modules-dir <path>` — specify modules directory (defaults to `./modules`)

You can also pass the modules directory positionally for backward compatibility:

```bash
./helixd --modules-dir ./modules
./helixd --socket /tmp/helixd.sock --foreground --interactive  # legacy CLI
```

Quick demo (non-interactive):

```bash
printf "install hello-module.helx\nenable hello-module\nstart hello-module\nstatus\nexit\n" | ./helixd ./modules
```

## Running as a background service

You can run the daemon as a background service and control it via the `helixctl` client.

Install a systemd service and socket activation (requires root):

```bash
sudo ./helixctl install-service --service-name helixd \
  --exec /usr/local/bin/helixd \
  --modules-dir /var/lib/helix/modules \
  --socket /run/helix/helixd.sock
```

`install-service` writes `/etc/systemd/system/helixd.service` and `/etc/systemd/system/helixd.socket`, runs `systemctl daemon-reload`, and enables/starts the socket (which will spawn helixd on demand). The daemon also supports systemd socket activation (LISTEN_FDS/LISTEN_PID) and sets the control socket permissions to 0666 when it manages the socket itself.

After the service is running, use `helixctl` to send commands:

```bash
./helixctl --socket /run/helix/helixd.sock status
./helixctl --socket /run/helix/helixd.sock list
./helixctl --socket /run/helix/helixd.sock install /path/to/module.helx
```

Tip: `./helixctl --version` prints the Helix core and API versions.

## Uninstall a module

From the daemon prompt:

```text
stop <name>
disable <name>
uninstall <name>
```

`uninstall` is dependency-aware and will refuse removal if other modules depend on it.

## Troubleshooting

- `helxcompiler`: header not found
  - Ensure it can see `include/helix/module.h`. Use `HELIX_ROOT` or pass `-I`.
- `helixd`: install fails from `.helx`
  - Ensure the archive has `manifest.json` and `lib<name>.so` at top level.
- Enable fails
  - Verify the module exports the expected symbols. Defaults are `helix_module_init`, `helix_module_start`, `helix_module_stop`, `helix_module_destroy`. If you use custom names, declare them in `manifest.json` under `entry_points` and rebuild the `.helx`.
- Name mismatch
  - The module name must match the `name` in `manifest.json`.

## Custom entry points

You can name your lifecycle functions and declare them in `manifest.json`:

```json
"entry_points": {
  "init": "my_init",
  "start": "my_start",
  "stop": "my_stop",
  "destroy": "my_destroy"
}
```

The compiler can also set these via flags:

- `--ep-init <symbol>`
- `--ep-start <symbol>`
- `--ep-stop <symbol>`
- `--ep-destroy <symbol>`

## Module state persistence

Helixd persists the last known module states to a small JSON file in the modules directory named `.helix_state.json`.

- On shutdown, it records whether each module was Installed, Initialized, Stopped, or Running.
- On the next startup, after scanning installed modules, helixd best-effort restores those states:
  - Modules previously Initialized/Stopped/Running are automatically enabled (with dependency resolution).
  - Modules previously Running are automatically started if enabling succeeds.
  - Missing modules or failures to enable/start are logged but do not block daemon startup.

Delete `.helix_state.json` to reset restoration behavior; helixd will then start with all modules in the Installed state.
