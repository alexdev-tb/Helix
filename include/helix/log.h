#ifndef HELIX_LOG_H
#define HELIX_LOG_H

#include <string>
#include <vector>
#include <mutex>

#ifdef __unix__
#include <dlfcn.h>
#endif

// Simple log level enum to keep C ABI minimal
enum HelixLogLevel {
    HELIX_LOG_DEBUG = 0,
    HELIX_LOG_INFO  = 1,
    HELIX_LOG_WARN  = 2,
    HELIX_LOG_ERROR = 3
};

#ifdef __unix__
// Function pointer types used by the logging registry
using HelixLogEmitFn = void (*)(const char*, int, const char*);
using HelixLogDispatchFn = void (*)(const char*, int, const char*);
using HelixLogRegisterSinkFn = void (*)(HelixLogEmitFn);
using HelixLogUnregisterSinkFn = void (*)(HelixLogEmitFn);
#endif

// Modules call this helper. It attempts to route to a logging module's
// helix_log_emit exported symbol. If unavailable, messages are queued in a
// bounded buffer and dropped on overflow. No printing from core.
inline void helix_log(const char* module_name, const char* message, HelixLogLevel level = HELIX_LOG_INFO) {
#ifdef __unix__
    struct PendingMsg { std::string mod; int lvl; std::string msg; };
    static HelixLogDispatchFn dispatch_fn = nullptr; // multi-sink dispatcher provided by daemon
    static std::mutex q_mtx;
    static std::vector<PendingMsg> queue; // lazily grows up to MAX
    static const size_t MAX = 256;

    auto try_resolve = [&]() {
        if (!dispatch_fn) {
            void* sym = dlsym(RTLD_DEFAULT, "helix_log_dispatch");
            if (sym) dispatch_fn = reinterpret_cast<HelixLogDispatchFn>(sym);
        }
        return dispatch_fn != nullptr;
    };

    if (!dispatch_fn && !try_resolve()) {
        // Queue message (best-effort, bounded)
        std::lock_guard<std::mutex> lock(q_mtx);
        if (queue.size() < MAX) {
            queue.push_back(PendingMsg{ module_name ? module_name : std::string("(unknown)"), static_cast<int>(level), message ? message : std::string() });
        }
        return; // no-op until logger is present
    }

    // If we reach here, dispatch_fn is available; flush any queued messages first
    {
        std::lock_guard<std::mutex> lock(q_mtx);
        for (const auto& pm : queue) {
            dispatch_fn(pm.mod.c_str(), pm.lvl, pm.msg.c_str());
        }
        queue.clear();
    }
    dispatch_fn(module_name ? module_name : "(unknown)", static_cast<int>(level), message ? message : "");
#else
    (void)module_name; (void)message; (void)level; // non-unix: no-op
#endif
}

// Helper accessors for logger modules to discover the registration functions.
// A logger module can do:
//   auto reg = helix_log_get_register(); if (reg) reg(my_sink);
inline HelixLogRegisterSinkFn helix_log_get_register() {
#ifdef __unix__
    void* sym = dlsym(RTLD_DEFAULT, "helix_log_register_sink");
    return sym ? reinterpret_cast<HelixLogRegisterSinkFn>(sym) : nullptr;
#else
    return nullptr;
#endif
}

inline HelixLogUnregisterSinkFn helix_log_get_unregister() {
#ifdef __unix__
    void* sym = dlsym(RTLD_DEFAULT, "helix_log_unregister_sink");
    return sym ? reinterpret_cast<HelixLogUnregisterSinkFn>(sym) : nullptr;
#else
    return nullptr;
#endif
}

#endif // HELIX_LOG_H
