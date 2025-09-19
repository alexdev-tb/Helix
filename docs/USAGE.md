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

- If your sources use `HELIX_MODULE_DECLARE`, name/version are auto-detected.
- The `.helx` tarball will contain `manifest.json` and `lib<name>.so` at the top level.

## Run the daemon

From the build dir:

```bash
mkdir -p modules
./helixd ./modules
```

While running, type commands at the prompt:

- `status` — show daemon status
- `list` — list installed modules
- `install <path>` — install from a `.helx` or a directory with manifest.json
- `enable <name>` — load and initialize
- `start <name>` — start a module
- `stop <name>` — stop a running module
- `disable <name>` — unload (calls destroy)
- `uninstall <name>` — remove files and unregister
- `help` — show commands
- `exit` — shutdown daemon

Quick demo (non-interactive):

```bash
printf "install modern_hello.helx\nenable modern-hello\nstart modern-hello\nstatus\nexit\n" | ./helixd ./modules
```

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
  - Verify the module exports: `helix_module_init`, `helix_module_start`, `helix_module_stop`, `helix_module_destroy`.
- Name mismatch
  - Must match `HELIX_MODULE_DECLARE("name", ...)` or the manifest name.
