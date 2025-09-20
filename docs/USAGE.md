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

## Compile a module to .helx

From within the build dir:

```bash
./helxcompiler -v -o modern_hello.helx ../examples/modern_hello/
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
# or equivalently
./helixd ./modules
```

Quick demo (non-interactive):

```bash
printf "install modern_hello.helx\nenable modern-hello\nstart modern-hello\nstatus\nexit\n" | ./helixd ./modules
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

You can name your lifecycle functions however you like and declare them in `manifest.json`:

```json
"entry_points": {
  "init": "my_init",
  "start": "my_start",
  "stop": "my_stop",
  "destroy": "my_destroy"
}
```

In code, use the short MDK macros to define custom symbols:

// HELIX*START(my_start) { /* ... _/ }
// HELIX_INIT(my_init) { /_ ... _/ return 0; }
// HELIX_STOP(my_stop) { /_ ... _/ return 0; }
// HELIX_DISABLE(my_destroy) { /_ ... \_/ }

The longer forms (`HELIX_MODULE_*_AS`) remain supported for backward compatibility.

The compiler can also set these via flags:

- `--ep-init <symbol>`
- `--ep-start <symbol>`
- `--ep-stop <symbol>`
- `--ep-destroy <symbol>`
