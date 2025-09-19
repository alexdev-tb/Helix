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

## Current status

Early alpha, working end-to-end:

- `helixd`: interactive daemon with install/enable/start/stop/disable/uninstall
- `helxcompiler`: compiles a module source directory into a `.helx` package
- Core libraries: `libhelix-core.a`, `libhelix-daemon.a`
- Examples: `examples/hello_module`, `examples/modern_hello`

We still follow a documentation-first mindset for significant changes (Issues → RFC → ADR),
but there is a functional prototype to try out.

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

## Quickstart

1. Build as above.

2. Create a `.helx` from an example module (from the build dir):

```bash
./helxcompiler -v -o modern_hello.helx ../examples/modern_hello/
```

3. Run the daemon and install your module:

```bash
mkdir -p modules
printf "install modern_hello.helx\nenable modern-hello\nstart modern-hello\nstatus\nexit\n" \
  | ./helixd ./modules
```

See `docs/USAGE.md` for more commands and troubleshooting.

## Module interface

Modules are shared libraries exporting these C symbols:

- `helix_module_init`
- `helix_module_start`
- `helix_module_stop`
- `helix_module_destroy`

For convenience, `include/helix/module.h` provides helper macros like
`HELIX_MODULE_DECLARE`, `HELIX_MODULE_INIT`, etc. The examples show both styles.

## Repository layout

- `include/helix/` — public headers (module API, daemon, loader)
- `src/` — core and daemon implementation
- `tools/helxcompiler/` — the module compiler
- `examples/` — example modules you can build and run
- `docs/` — usage guide, architecture notes, ADR/RFC templates
- `api/` — API docs pointers and index
- `CHANGELOG.md` — notable changes

## Contributing

We use an Issue → RFC (`docs/rfcs/`) → ADR (`docs/adr/`) workflow for substantial
changes. Please open an Issue first, and update docs alongside any code.

Coding style: keep public APIs stable, prefer small, reviewable changes, and
update `CHANGELOG.md` when behavior is user-visible.

## License

MIT — see `LICENSE`.
