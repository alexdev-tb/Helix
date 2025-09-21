# Helix — Userspace Module Framework for Linux

Helix is a userspace, microkernel-style framework: a small, stable core with pluggable,
runtime-loadable modules. It runs entirely in user space and provides a simple API so
developers can build, package, and run compiled modules that extend the platform and
each other.

## Highlights

- Userspace daemon and core libraries
- Dynamic module loading via shared objects (.so)
- Standard module package format (.helx = tar.gz with manifest + binary)
- Module lifecycle: install, enable, start, stop, disable, uninstall
- Dependency resolution from manifests
- Example modules and a module compiler
- Pluggable, multi-sink logging via logger modules (modules call `helix_log()`; sinks subscribe)

## Current status

Early alpha, working end-to-end:

- `helixd`: interactive daemon with install/enable/start/stop/disable/uninstall
- `helxcompiler`: compiles a module source directory into a `.helx` package
- Core libraries: `libhelix-core.a`, `libhelix-daemon.a`
- Examples: `examples/hello_module`

## Build

Prerequisites: CMake ≥ 3.16, a C++17 compiler, tar.

Using CMake presets from the repo root:

```bash
cmake --preset release
cmake --build --preset release -j
```

Artifacts appear under `build-release/` (or `build-debug/` for the debug preset):

- `helixd` — daemon
- `helxcompiler` — module compiler producing `.helx`
- `libhelix-core.a`, `libhelix-daemon.a`

Versioning is centralized via CMake and a generated header. See `cmake/Version.cmake` if you
need to override the version when packaging releases.

## Quickstart

1. Build as above.

2. Create a `.helx` from an example module (from the build dir):

```bash
./helxcompiler -v -o hello-module.helx ../modules/examples/hello_module/
```

3. Run the daemon and install your module:

```bash
mkdir -p modules
printf "install hello-module.helx\nenable hello-module\nstart hello-module\nstatus\nexit\n" \
  | ./helixd --modules-dir ./modules --foreground --interactive
```

See `docs/USAGE.md` for more commands and troubleshooting.

## Module interface

Modules are shared libraries exporting these C symbols:

- `helix_module_init`
- `helix_module_start`
- `helix_module_stop`
- `helix_module_destroy`

For convenience, `include/helix/module.h` provides helper macros like
`HELIX_MODULE_INIT`, etc. Metadata (name/version/description/author) is read from `manifest.json`,
so `HELIX_MODULE_DECLARE` is optional.

Lifecycle performance: lifecycle calls (`init/start/stop/destroy`) are synchronous on the daemon’s control thread. Keep them short and non-blocking; do long-running work in your own threads and return promptly. See `docs/architecture.md#concurrency-and-threading-model`.

## Repository layout

- `include/helix/` — public headers (module API, daemon, loader)
- `src/` — core and daemon implementation
- `tools/helxcompiler/` — the module compiler
- `modules/examples/` — example modules you can build and run
- `api/` — API docs pointers and index
- `CHANGELOG.md` — notable changes

Logging API overview: `include/helix/log.h` provides `helix_log()` which queues until a Logger module registers a sink and then dispatches to all sinks. The framework doesn’t print logs by itself.

## Contributing

Please open an Issue first, and update docs alongside any code.

Coding style: keep public APIs stable, prefer small, reviewable changes, and
update `CHANGELOG.md` when behavior is user-visible.

## License

MIT — see `LICENSE`.

## Help

Need more help? You can refer to the [USAGE guide](docs/USAGE.md) or open an issue on GitHub.
