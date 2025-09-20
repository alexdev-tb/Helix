---
applyTo: "**"
---

# Helix – Assistant Working Guide

This file gives you (the AI assistant) the context and guardrails you need to generate code, answer questions, and review changes for this repository.

## Quick facts

- Language: C++17
- Build system: CMake (presets: `debug`, `release`)
- Targets: `helixd` (daemon with IPC), `helixctl` (CLI client), `helxcompiler` (module compiler), `helix-core`/`helix-daemon` (static libs)
- OS: Linux; shell: bash
- Modules: runtime-loaded `.so` packaged as `.helx` (tar.gz with `manifest.json` + `lib<name>.so`)
- Default module entry points: `helix_module_init`, `helix_module_start`, `helix_module_stop`, `helix_module_destroy` (customizable via `entry_points` in manifest)

Key references:

- Overview: `README.md`
- Usage: `docs/USAGE.md`
- Architecture: `docs/architecture.md`
- Public headers (APIs): `include/helix/`
- Compiler tool: `tools/helxcompiler/`
- Examples: `modules/examples/`

## Project overview

Helix is a userspace, microkernel-style framework. A small core manages lifecycle for pluggable modules that are compiled as shared objects and loaded dynamically by the daemon. Modules declare metadata and (optionally) custom entry symbols via a manifest.

Core components:

- Module loader (`include/helix/module_loader.h`): dlopen/dlsym wrapper + lifecycle calls
- Manifest model/parser (`include/helix/manifest.h`)
- Dependency resolver (`include/helix/dependency_resolver.h`)
- Daemon (`include/helix/daemon.h`, `src/daemon/`): IPC server with a legacy interactive CLI for install/enable/start/stop/disable/uninstall; control via `helixctl`
- Module compiler (`tools/helxcompiler/`): builds `.so` and packages `.helx`

## Build and run

From the repo root using CMake presets:

```bash
cmake --preset debug
cmake --build --preset debug -j

# or
cmake --preset release
cmake --build --preset release -j
```

Artifacts (under `build-<preset>/`):

- `helixd` – daemon (IPC server; add `--interactive` for legacy CLI)
- `helixctl` – CLI client controlling `helixd` via a Unix socket
- `helxcompiler` – module compiler producing `.helx`
- `libhelix-core.a`, `libhelix-daemon.a`

Run the daemon (modules dir default is `./modules`):

```bash
mkdir -p modules
./helixd --modules-dir ./modules --socket /tmp/helixd.sock --foreground
```

Daemon commands (from `src/daemon/main.cpp`, available over IPC and in interactive mode):

// control via `helixctl` (e.g., `./helixctl --socket /tmp/helixd.sock status`)

- `status` – show daemon status
- `version` – show core/api versions
- `list` – list installed modules
- `info <name>` – show module details
- `install <file.helx>` – install from package
- `enable <name>` – load + init
- `start <name>` – start
- `stop <name>` – stop
- `disable <name>` – unload (destroy)
- `uninstall <name>` – remove (fails if dependents exist)
- `help` – list commands
- `quit`/`exit` – shutdown

## Module development kit (MDK)

Public header: `include/helix/module.h`

- Declare metadata: `HELIX_MODULE_DECLARE(name, version, description, author)` (optional; manifest is source of truth)
- Default entry points: define with `HELIX_MODULE_INIT()`, `HELIX_MODULE_START()`, `HELIX_MODULE_STOP()`, `HELIX_MODULE_DESTROY()`
- Custom symbols: `HELIX_MODULE_*_AS(symbol)` macros
- Helpers: `HELIX_MODULE_NAME`, `HELIX_MODULE_VERSION`, `HELIX_MODULE_CONTEXT()`, logging macros

The loader (`include/helix/module_loader.h`) expects the four lifecycle symbols. It supports custom names from the manifest `entry_points` field.

## Module packaging (.helx)

A `.helx` is a tar.gz containing at the top level:

- `manifest.json`
- `lib<name>.so`

Minimal manifest fields (see `include/helix/manifest.h` and examples in `modules/examples/*/manifest.json`):

```json
{
  "name": "modern-hello",
  "version": "1.0.0",
  "description": "...",
  "author": "...",
  "license": "MIT",
  "binary_path": "libmodern-hello.so",
  "minimum_core_version": "2.1.0",
  "minimum_api_version": "2.0.0",
  "dependencies": [],
  "capabilities": [],
  "config": {},
  "entry_points": {
    "init": "helix_module_init",
    "start": "helix_module_start",
    "stop": "helix_module_stop",
    "destroy": "helix_module_destroy"
  }
}
```

Note: `entry_points` is optional; defaults apply if omitted. When custom names are used in code, they must match the manifest.

## Using the compiler (helxcompiler)

Build a module directory into a `.helx` (from the build dir):

```bash
./helxcompiler -v -o hello-module.helx ../modules/examples/hello_module/
```

Supported flags (from `tools/helxcompiler/main.cpp`):

- `-o, --output <file>` – output `.helx`
- `-n, --name <name>` – module name (auto-detected if omitted)
- `-V, --version <ver>` – module version (auto-detected if omitted)
- `-I, --include <path>` – add include path
- `-L, --library-path <path>` – add library search path
- `-l, --library <lib>` – link against library
- `--std <standard>` – C++ standard (default c++17)
- `-O, --optimize <level>` – optimization level (default -O2)
- `-g, --debug` – include debug info
- `-v, --verbose` – verbose output
- `--ep-init|--ep-start|--ep-stop|--ep-destroy <symbol>` – set custom entry symbols
- `--validate` – validate `manifest.json` in source dir only (no build)

It auto-detects the Helix include path via `HELIX_ROOT` or by walking up to find `include/helix/module.h`.

## Dependency resolution

`include/helix/dependency_resolver.h` provides version-aware resolution with cycle and missing-dependency detection. The daemon uses it to determine load order and to block unsafe uninstalls.

## Repository structure

- `include/helix/` – public API headers
- `src/core/` – core implementations (manifest, loader, resolver)
- `src/daemon/` – daemon implementation and CLI
- `tools/helxcompiler/` – module compiler sources
- `modules/examples/` – example modules (`hello_module`, `modern_hello`)
- `docs/` – `USAGE.md`, `architecture.md`, ADR/RFC templates
- `api/` – API index pointing to public headers

## Conventions and guardrails (for assistant)

Do:

- Prefer minimal, well-scoped changes; keep public APIs in `include/helix/` stable unless explicitly asked to change them.
- Use C++17 and standard libraries; link `-pthread -ldl` for module/daemon work.
- Follow existing CMake patterns; add sources to top-level `CMakeLists.txt` or nested `CMakeLists.txt` as appropriate. Use presets.
- Update docs alongside behavior changes: `docs/USAGE.md`, `README.md`, and `CHANGELOG.md` when user-visible.
- Keep `.helx` format simple: top-level `manifest.json` + `lib<name>.so`.

Avoid:

- Adding heavy third-party dependencies without a compelling reason.
- Breaking the MDK macros or default entry point expectations.
- Introducing OS-specific behavior (project targets Linux userspace).

## Common tasks playbook

- Create a new example module:

  1.  Scaffold `examples/<module>/` with one `.cpp` using MDK macros.
  2.  Optionally add a `manifest.json` (or let the compiler generate one).
  3.  Build `.helx` via `helxcompiler` and install via `helixd`.

- Add a daemon command:

  1.  Update `src/daemon/main.cpp` command loop.
  2.  Implement behavior via `helix::HelixDaemon` in `include/helix/daemon.h`/`src/daemon/daemon.cpp`.
  3.  Document in `docs/USAGE.md`.

- Extend manifest schema validation:
  1.  Update `include/helix/manifest.h` types as needed.
  2.  Implement parse/validate logic in `src/core/manifest.cpp`.
  3.  Ensure `helxcompiler` continues to generate valid manifests.

## IPC and control client

`helixd` exposes a Unix domain socket for control (default `/tmp/helixd.sock`, configurable via `--socket`). Use `helixctl` to send commands, or run `helixd --interactive` for an inline CLI. A systemd service can be installed via `helixctl install-service`.

## Glossary

- Helix core: libraries and daemon that manage modules
- Module: shared library with lifecycle entry points
- MDK: module development kit (`include/helix/module.h`)
- `.helx`: tar.gz package with `manifest.json` + module `.so`
- Daemon: `helixd` interactive manager

## Useful links

- `README.md`
- `docs/USAGE.md`
- `docs/architecture.md`
- `api/README.md`
- `modules/examples/`

If any assumption is unclear, read the corresponding header in `include/helix/` and prefer aligning with existing patterns.
