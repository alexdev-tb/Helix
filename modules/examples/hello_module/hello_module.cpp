#include <iostream>
#include "helix/log.h"
#include "helix/module.h"

// Helix module interface implementation
// All modules must implement these four entry points

// Provide module metadata for tooling and manifests
HELIX_MODULE_DECLARE("hello-module", "1.0.0", "Example Hello module", "Helix Team")

extern "C" {

/**
 * Initialize the module
 * Called once when the module is first loaded
 */
int helix_module_init() {
    helix_log("HelloModule", "Initializing...", HELIX_LOG_INFO);
    return 0; // 0 = success
}

/**
 * Start the module
 * Called when the module should begin its main operation
 */
int helix_module_start() {
    helix_log("HelloModule", "Starting...", HELIX_LOG_INFO);
    helix_log("HelloModule", "Hello from Helix module!", HELIX_LOG_INFO);
    return 0; // 0 = success
}

/**
 * Stop the module
 * Called when the module should stop its main operation
 */
int helix_module_stop() {
    helix_log("HelloModule", "Stopping...", HELIX_LOG_INFO);
    return 0; // 0 = success
}

/**
 * Destroy the module
 * Called once when the module is being unloaded
 */
void helix_module_destroy() {
    helix_log("HelloModule", "Cleaning up...", HELIX_LOG_DEBUG);
}

} // extern "C"