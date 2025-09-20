#include <helix/module.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <cstdio>


// Global module state
static std::atomic<bool> module_running{false};
static std::thread worker_thread;

// Runtime-configurable settings and stats
static std::mutex mtx;
static std::condition_variable cv;
static std::chrono::seconds interval_s{5};
static std::string base_message{"Hello from Helix!"};
static uint64_t message_count = 0;

// Worker loop
static void worker_loop() {
    HELIX_MODULE_LOG("Worker thread started");
    try {
        std::unique_lock<std::mutex> lock(mtx);
        while (module_running.load(std::memory_order_acquire)) {
            // Wait for interval or a configuration change/stop
            cv.wait_for(lock, interval_s, [] {
                return !module_running.load(std::memory_order_relaxed);
            });
            if (!module_running.load(std::memory_order_relaxed)) break;

            ++message_count;
            HELIX_MODULE_LOG(base_message + " (message #" + std::to_string(message_count) + ")");
        }
    } catch (const std::exception& ex) {
        HELIX_MODULE_ERROR(std::string("Worker thread exception: ") + ex.what());
    } catch (...) {
        HELIX_MODULE_ERROR("Worker thread encountered unknown exception");
    }
    HELIX_MODULE_LOG("Worker thread stopping...");
}

// Helpers to read env
static void apply_env_overrides() {
    if (const char* envMsg = std::getenv("HELIX_HELLO_MESSAGE")) {
        std::lock_guard<std::mutex> lock(mtx);
        if (*envMsg) base_message = envMsg;
    }
    if (const char* envInt = std::getenv("HELIX_HELLO_INTERVAL")) {
        char* end = nullptr;
        unsigned long v = std::strtoul(envInt, &end, 10);
        if (end && *end == '\0' && v > 0 && v <= 86400UL) {
            std::lock_guard<std::mutex> lock(mtx);
            interval_s = std::chrono::seconds(static_cast<unsigned int>(v));
        } else {
            HELIX_MODULE_ERROR("Invalid HELIX_HELLO_INTERVAL, keeping default");
        }
    }
}

// Module initialization (custom symbol name)
HELIX_INIT(my_init) {
    HELIX_MODULE_LOG("Initializing modern hello module...");

    // Initialize any resources here
    auto context = HELIX_MODULE_CONTEXT();
    HELIX_MODULE_LOG("Module " + context.module_name + " v" + context.module_version + " initialized");

    apply_env_overrides();
    {
        std::lock_guard<std::mutex> lock(mtx);
        HELIX_MODULE_LOG("Initial message: " + base_message);
        HELIX_MODULE_LOG("Initial interval: " + std::to_string(interval_s.count()) + "s");
    }

    return 0; // Success
}

// Module start function (custom symbol name)
HELIX_START(my_start) {
    HELIX_MODULE_LOG("Starting modern hello module...");

    if (module_running.load(std::memory_order_acquire)) {
        HELIX_MODULE_ERROR("Module is already running!");
        return 1; // Error
    }

    module_running.store(true, std::memory_order_release);

    // Start a background worker thread
    worker_thread = std::thread(worker_loop);

    HELIX_MODULE_LOG("Modern hello module started successfully");
    return 0; // Success
}

// Module stop function (custom symbol name)
HELIX_STOP(my_stop) {
    HELIX_MODULE_LOG("Stopping modern hello module...");

    if (!module_running.load(std::memory_order_acquire)) {
        HELIX_MODULE_ERROR("Module is not running!");
        return 1; // Error
    }

    module_running.store(false, std::memory_order_release);
    cv.notify_all();

    // Wait for worker thread to finish
    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    HELIX_MODULE_LOG("Modern hello module stopped successfully");
    return 0; // Success
}

// Module cleanup function (custom symbol name)
HELIX_DISABLE(my_destroy) {
    HELIX_MODULE_LOG("Cleaning up modern hello module...");

    // Ensure module is stopped
    if (module_running.load(std::memory_order_acquire)) {
        module_running.store(false, std::memory_order_release);
        cv.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    HELIX_MODULE_LOG("Modern hello module cleanup complete");
}

// Public C ABI for runtime control and stats
extern "C" {

struct modern_hello_stats {
    uint64_t total_messages;
    uint32_t interval_seconds;
    char message[128];
};

// Update the interval (in seconds, min 1, max 86400)
void modern_hello_set_interval(uint32_t seconds) {
    if (seconds == 0) seconds = 1;
    if (seconds > 86400) seconds = 86400;
    {
        std::lock_guard<std::mutex> lock(mtx);
        interval_s = std::chrono::seconds(seconds);
    }
    cv.notify_all();
    HELIX_MODULE_LOG("Interval updated to " + std::to_string(seconds) + "s");
}

// Update the base message
void modern_hello_set_message(const char* msg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (msg && *msg) {
            base_message = msg;
        } else {
            base_message = "Hello from Helix!";
        }
        HELIX_MODULE_LOG("Message updated to: " + base_message);
    }
}

// Emit an immediate message (does not wait for interval)
void modern_hello_say(const char* msg) {
    std::string to_say;
    {
        std::lock_guard<std::mutex> lock(mtx);
        to_say = (msg && *msg) ? std::string(msg) : base_message;
        ++message_count;
    }
    HELIX_MODULE_LOG(to_say + " (message #" + std::to_string(message_count) + ")");
}

// Retrieve current stats
void modern_hello_get_stats(modern_hello_stats* out) {
    if (!out) return;
    std::lock_guard<std::mutex> lock(mtx);
    out->total_messages = message_count;
    out->interval_seconds = static_cast<uint32_t>(interval_s.count());
    std::snprintf(out->message, sizeof(out->message), "%s", base_message.c_str());
}

} // extern "C"