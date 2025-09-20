# Contributing to Helix

Thanks for your interest in contributing! Helix follows a documentation-first approach for significant changes, and has an active codebase. Please coordinate via Issues, RFCs, and ADRs, and update docs alongside code.

## How to contribute

- Open an Issue to discuss ideas or problems
- For substantial proposals, submit an RFC using the template in `docs/rfcs/0000-template.md`
- For architectural decisions, submit an ADR under `docs/adr/`
- Keep PRs small and focused; update docs and `CHANGELOG.md` for user-visible changes

## Coding guidelines

- C++17, prefer standard library over custom utilities
- Keep public APIs stable; document any behavioral changes in `api/README.md`
- Lifecycle calls should be kept short; modules must spawn their own worker threads for long-running tasks
- Add minimal tests or runnable examples where feasible
- Build locally with presets: `cmake --preset debug && cmake --build --preset debug -j`

## Versioning

- Core version and API version are configured in `cmake/Version.cmake` or via `-DHELIX_VERSION`/`-DHELIX_API_VERSION`
- The generated header `helix/version.h` exposes these versions to C++

## Commit and PR process

- Reference the Issue/RFC/ADR in your PR description
- Update relevant docs: `README.md`, `docs/USAGE.md`, `docs/architecture.md`, `api/README.md`
- Update `CHANGELOG.md` under `[Unreleased]` with concise bullets

## Code of Conduct

Be respectful and constructive. We welcome new contributors.
