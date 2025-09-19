# Helix Framework - AI Coding Agent Instructions

## Project Architecture

**Core Concept**: A userspace "microkernel-style" framework where a small daemon manages dynamically loadable compiled modules (.so files) with standardized packaging (.helx format).

**Key Components**:

- **Daemon**: Module lifecycle management (install/enable/start/stop/disable/uninstall)
- **CLI (helxctl)**: User interface for module operations
- **Module Format (.helx)**: Standardized packaging with manifest + binary
- **Packaging Tool (mkmod)**: Developer tool for creating .helx packages
- **Inter-module Communication**: Event bus/message API between modules and core

## Development Workflow

This is a **documentation-first project**. Follow this development approach:

1. **No implementation code yet** - Focus on docs, specs, and architectural planning
2. **Use Issue → RFC → ADR workflow** for substantial changes:
   - Issues for discussion and problem identification
   - RFCs (`docs/rfcs/`) for feature proposals using template
   - ADRs (`docs/adr/`) for architectural decisions using template
3. **Maintain documentation structure** mentioned in README.md even if directories don't exist yet

## Project Conventions

### Documentation Standards

- **Markdown-focused**: Uses markdownlint, YAML validation, drawio integration
- **100-character line rulers** per `.vscode/settings.json`
- **Issue/PR templates** enforce structured contributions
- **Changelog discipline**: Update `CHANGELOG.md` for notable changes

### Recommended Extensions

Per `.vscode/extensions.json`:

- `DavidAnson.vscode-markdownlint` - Markdown linting
- `redhat.vscode-yaml` - YAML validation
- `hediet.vscode-drawio` - Architecture diagrams
- `Gruntfuggly.todo-tree` - TODO tracking

### File Organization Patterns

Follow the structure outlined in `README.md`:

- `docs/` - Conceptual documentation, architecture, ADRs, RFCs
- `specs/` - Specifications and schemas (module format, ABI, etc.)
- `api/` - Runtime and events interface documentation
- `modules/examples/` - Example module layouts
- `configs/` - Sample configuration files

## Key Design Principles

1. **Userspace Only**: No kernel modules, fully userspace implementation
2. **Microkernel Philosophy**: Small stable core + pluggable extensions
3. **Dynamic Loading**: Runtime module loading via dlopen() with standard entry points
4. **Dependency Resolution**: Manifest-driven dependency graph management
5. **Isolation & Security**: Process/thread separation with capability-based permissions

## Contributing Guidelines

- **Open Issues first** before significant work
- **Use RFC process** for new features or substantial changes
- **Follow ADR process** for architectural decisions
- **Update documentation** alongside any structural changes
- **Respect documentation-first approach** - avoid adding implementation until specs exist

When creating new components, reference the planned structure in `README.md` and ensure alignment with the microkernel-style architecture.
