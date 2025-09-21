# Examples

This folder contains example Helix modules:

- `hello_module/` — Classic minimal module using default entry point symbols
  - Exports: helix_module_init/start/stop/destroy
  - Manifest does not specify `entry_points` (defaults apply)

Build the project and use `helxcompiler` to package them into `.helx`, or install the built artifacts into the daemon's modules directory for testing.

Example compile from build dir:

```bash
./helxcompiler -v -o hello-module.helx ../modules/examples/hello_module/
```

Note on runtime behavior: the daemon invokes module lifecycle functions synchronously. The examples' `start()` functions return immediately after logging; real modules should spawn their own worker thread(s) in `start()` and return promptly to avoid blocking other operations. On `stop()`, signal and join your threads with a bounded wait.
