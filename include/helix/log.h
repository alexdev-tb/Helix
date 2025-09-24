#ifndef HELIX_LOG_H
#define HELIX_LOG_H

#include <string>
#include <cstdint>

#ifdef __unix__
#include <dlfcn.h>
#endif

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

// Stats structure shared with the registry. Optional: will be zeroed if not supported.
struct HelixLogStats {
    uint64_t dispatched;        // total messages dispatched to sinks (post-filter)
    uint64_t dropped;           // total dropped for any reason
    uint64_t dropped_overflow;  // dropped due to bounded pre-sink queue overflow
    uint64_t dropped_filtered;  // dropped due to level filter
    uint64_t queued;            // current pre-sink queue size
    uint64_t queue_capacity;    // configured capacity for pre-sink queue
    uint64_t sinks;             // number of registered sinks
    int      min_level;         // current minimum level filter
};

using HelixLogGetStatsFn = void (*)(HelixLogStats*);
using HelixLogSetMinLevelFn = void (*)(int);
using HelixLogGetMinLevelFn = int (*)();
#endif

// Modules call this helper. It attempts to route to a logging module's
// helix_log_emit exported symbol via a central dispatcher. The central
// registry manages pre-sink buffering, filtering, and stats. No printing here.
inline void helix_log(const char* module_name, const char* message, HelixLogLevel level = HELIX_LOG_INFO) {
#ifdef __unix__
    static HelixLogDispatchFn dispatch_fn = nullptr;
    if (!dispatch_fn) {
        void* sym = dlsym(RTLD_DEFAULT, "helix_log_dispatch");
        if (sym) dispatch_fn = reinterpret_cast<HelixLogDispatchFn>(sym);
    }
    if (dispatch_fn) {
        dispatch_fn(module_name ? module_name : "(unknown)", static_cast<int>(level), message ? message : "");
    }
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

// Optional: query central logging stats; returns false if unsupported.
inline bool helix_log_get_stats(struct HelixLogStats* out) {
#ifdef __unix__
    if (!out) return false;
    void* sym = dlsym(RTLD_DEFAULT, "helix_log_stats_get");
    if (!sym) { *out = HelixLogStats{}; return false; }
    auto fn = reinterpret_cast<HelixLogGetStatsFn>(sym);
    fn(out);
    return true;
#else
    (void)out; return false;
#endif
}

inline void helix_log_set_min_level(HelixLogLevel level) {
#ifdef __unix__
    void* sym = dlsym(RTLD_DEFAULT, "helix_log_min_level_set");
    if (sym) reinterpret_cast<HelixLogSetMinLevelFn>(sym)(static_cast<int>(level));
#else
    (void)level;
#endif
}

inline HelixLogLevel helix_log_get_min_level() {
#ifdef __unix__
    void* sym = dlsym(RTLD_DEFAULT, "helix_log_min_level_get");
    if (sym) return static_cast<HelixLogLevel>(reinterpret_cast<HelixLogGetMinLevelFn>(sym)());
#endif
    return HELIX_LOG_INFO;
}

#endif // HELIX_LOG_H
