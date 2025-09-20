# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

- Architecture docs and API index
- RFC/ADR templates for contribution workflow
- Additional examples and docs improvements

Docs refresh:

- Updated paths to examples under `modules/examples/` across README, USAGE, src/README, and assistant guide.
- Documented `helixctl`, IPC socket control, and systemd service install.
- Clarified logging flow and dispatcher symbol in API and architecture docs.
- Corrected quickstart commands and `helxcompiler` usage examples.
- Refreshed assistant working guide under `.github/instructions/instructions.md` to be accurate.

Docs:

- Architecture: added "Concurrency and threading model" section documenting synchronous lifecycle and guidance for non-blocking modules.
- API: expanded logging API docs (`helix/log.h` queueing/dispatch, sink registration, levels, threading notes).
- Usage: added note on keeping lifecycle non-blocking; clarified logging behavior.
- README: highlighted multi-sink logging; linked to concurrency section.
- CONTRIBUTING: updated to reflect active codebase and docs-first workflow; added coding/versioning guidelines.

Improvements:

- helixd: add `-h/--help` and `--modules-dir <path>` flags; print usage and exit
  when `--help` is provided.
- helixd: add `--version` flag to print the Helix core version and exit.
- New `helixctl` client to control the daemon over a Unix socket.
- `helixctl install-service` installs a systemd service unit and enables/starts it.
- helixd IPC/list and info responses: `list` now returns `<name> <State>` and
  `info` includes extended manifest metadata (description, author, license,
  binary_path, minimum_core_version, minimum_api_version).
- Systemd socket activation: helixd recognizes LISTEN_FDS/LISTEN_PID and can be
  activated by a `.socket` unit; when managing its own socket, it sets mode 0666
  for non-root clients.
- helixctl UX: colorized output for `list` and `info`, plus `--no-color` flag;
  pretty-printed key/value output for `info`.
- helixctl reliability: absolute path normalization for `install <file.helx>` and
  improved error propagation with human-readable reasons.
- helixctl install-service now also writes a matching `helixd.socket` unit and
  enables/starts the socket (falls back to enabling the service if socket fails).
- MDK: `HELIX_MODULE_DECLARE` is now optional. Module metadata is sourced from `manifest.json`. Examples updated.
- MDK: Added short alias macros `HELIX_INIT`, `HELIX_START`, `HELIX_STOP`, `HELIX_DISABLE` for cleaner entry point declarations (backward compatible).
- Tooling/IDE: CMake presets now export `compile_commands.json` to improve
  IntelliSense/autocomplete in editors.
- Build: Centralized version management via `cmake/Version.cmake`.
  Set `-DHELIX_VERSION=MAJOR.MINOR.PATCH` at configure time to propagate version
  to all targets and generated header `helix/version.h`.
  Also introduces a separate `HELIX_API_VERSION` (default `1.0.0`) for the public
  module API. Both appear in the generated header and can be overridden at
  configure time (`-DHELIX_API_VERSION=...`).
- helxcompiler: if `manifest.json` omits `minimum_api_version` or `minimum_core_version`,
  they are defaulted to `HELIX_API_VERSION` and `HELIX_VERSION` respectively.
- helixd: enforce `minimum_api_version` at install time; refuse install if the daemon's API
  version (HELIX_API_VERSION) is lower than the module requires.
- helixctl: add `--version` to print Helix core and API versions.

Bug fixes:

- helixd: avoid treating `--help` as a modules directory argument.

## [0.2.0] - 2025-09-19

Features:

- helixd daemon: interactive command loop with install/enable/start/stop/disable/uninstall
- Module compiler (helxcompiler): compile sources to `.so` and package as `.helx`
- Core libraries: `libhelix-core.a` and `libhelix-daemon.a`
- Example modules: classic `hello_module` and macro-based `modern_hello`

Improvements:

- DependencyResolver with basic missing/cycle detection and load ordering
- Manifest parsing and module registry management in the daemon
- ModuleLoader with strict entry-point resolution and lifecycle management
- CMake presets (Debug/Release) and improved build docs
- Usage guide with troubleshooting and non-interactive demo

Docs:

- Updated README with quickstart and module interface
- New `docs/architecture.md`
- New `api/README.md` index

Internal:

- Helper macros and context in `include/helix/module.h`
