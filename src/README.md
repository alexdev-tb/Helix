# Helix Core Implementation

This directory contains the C++ implementation of the Helix core and daemon.

## Architecture

Microkernel-style:

- Small core: module loading, dependency resolution, lifecycle management
- Pluggable extensions: dynamically loaded modules (.so)
- Userspace only: no kernel modules required

## Core components

- ModuleLoader (`src/core/module_loader.cpp`): loads/unloads `.so`, resolves entry points,
  manages initialize/start/stop/destroy.
- Manifest (`src/core/manifest.cpp`): parses `manifest.json` in module packages.
- DependencyResolver (`src/core/dependency_resolver.cpp`): build graph, detect cycles,
  compute load order, check missing deps.
- Daemon (`src/daemon/daemon.cpp`): install/enable/start/stop/disable/uninstall;
  tracks module state and uses the loader/resolver.

## Module interface

Modules export these C functions:

```cpp
extern "C" {
    int  helix_module_init();
    int  helix_module_start();
    int  helix_module_stop();
    void helix_module_destroy();
}
```

Or use helpers in `include/helix/module.h` (`HELIX_MODULE_DECLARE`, `HELIX_MODULE_INIT`, etc.).

## Build

From the repo root, prefer CMake presets:

```bash
cmake --preset release && cmake --build --preset release -j
```

Outputs: `helixd`, `helxcompiler`, `libhelix-core.a`, `libhelix-daemon.a` under `build-release/`.

## Run

From the build dir:

```bash
mkdir -p modules
./helixd ./modules
```

Useful daemon commands: `status`, `list`, `install <path>`, `enable <name>`,
`start <name>`, `stop <name>`, `disable <name>`, `uninstall <name>`, `help`, `exit`.

## Examples

See `examples/hello_module` (raw C API) and `examples/modern_hello` (macro helpers).
Use `tools/helxcompiler` to build a `.helx`:

```bash
./helxcompiler -v -o modern_hello.helx ../examples/modern_hello/
```

## Module package format

`.helx` is a tar.gz containing at the top level:

- `manifest.json`
- `lib<name>.so`

The daemon can also install from an unpacked directory with the same layout.

## Roadmap (high level)

- Event bus / inter-module messaging
- Capability-based permissions
- Remote/CLI control and service mode
- Hot reload and live updates
