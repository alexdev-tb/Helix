#include <helix/module.h>
#include <iostream>
#include <thread>
#include <chrono>

// Declare module metadata
HELIX_MODULE_DECLARE(
    "modern-hello",
    "2.0.0", 
    "A modern hello world module using the Helix module development kit",
    "Helix Framework Team"
)

// Declare module capabilities (this module needs no special permissions)
HELIX_MODULE_CAPABILITIES()

// Global module state
static bool module_running = false;
static std::thread worker_thread;

// Module initialization
HELIX_MODULE_INIT() {
    HELIX_MODULE_LOG("Initializing modern hello module...");
    
    // Initialize any resources here
    auto context = HELIX_MODULE_CONTEXT();
    HELIX_MODULE_LOG("Module " + context.module_name + " v" + context.module_version + " initialized");
    
    return 0; // Success
}

// Module start function
HELIX_MODULE_START() {
    HELIX_MODULE_LOG("Starting modern hello module...");
    
    if (module_running) {
        HELIX_MODULE_ERROR("Module is already running!");
        return 1; // Error
    }
    
    module_running = true;
    
    // Start a background worker thread
    worker_thread = std::thread([]() {
        int count = 0;
        while (module_running) {
            HELIX_MODULE_LOG("Hello from Helix! (message #" + std::to_string(++count) + ")");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        HELIX_MODULE_LOG("Worker thread stopping...");
    });
    
    HELIX_MODULE_LOG("Modern hello module started successfully");
    return 0; // Success
}

// Module stop function
HELIX_MODULE_STOP() {
    HELIX_MODULE_LOG("Stopping modern hello module...");
    
    if (!module_running) {
        HELIX_MODULE_ERROR("Module is not running!");
        return 1; // Error
    }
    
    module_running = false;
    
    // Wait for worker thread to finish
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    HELIX_MODULE_LOG("Modern hello module stopped successfully");
    return 0; // Success
}

// Module cleanup function
HELIX_MODULE_DESTROY() {
    HELIX_MODULE_LOG("Cleaning up modern hello module...");
    
    // Ensure module is stopped
    if (module_running) {
        module_running = false;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    HELIX_MODULE_LOG("Modern hello module cleanup complete");
}