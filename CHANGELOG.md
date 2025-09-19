# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

- Architecture docs and API index
- RFC/ADR templates for contribution workflow
- Additional examples and docs improvements

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
