#include <iostream>

// Helix module interface implementation
// All modules must implement these four entry points

extern "C" {

/**
 * Initialize the module
 * Called once when the module is first loaded
 */
int helix_module_init() {
    std::cout << "[HelloModule] Initializing..." << std::endl;
    return 0; // 0 = success
}

/**
 * Start the module
 * Called when the module should begin its main operation
 */
int helix_module_start() {
    std::cout << "[HelloModule] Starting..." << std::endl;
    std::cout << "[HelloModule] Hello from Helix module!" << std::endl;
    return 0; // 0 = success
}

/**
 * Stop the module
 * Called when the module should stop its main operation
 */
int helix_module_stop() {
    std::cout << "[HelloModule] Stopping..." << std::endl;
    return 0; // 0 = success
}

/**
 * Destroy the module
 * Called once when the module is being unloaded
 */
void helix_module_destroy() {
    std::cout << "[HelloModule] Cleaning up..." << std::endl;
}

} // extern "C"