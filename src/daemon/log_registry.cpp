#include <vector>
#include <mutex>
#include <algorithm>
#include <deque>
#include <atomic>
#include <tuple>
#include <string>
// Centralized logging registry with bounded pre-sink queue, stats, and level filtering.
// Environment variables influence behavior:
// - HELIX_LOG_QUEUE_CAP: capacity of pre-sink queue (default 256)
// - HELIX_LOG_MIN_LEVEL: 0=DEBUG,1=INFO,2=WARN,3=ERROR (default 1)
#include <cstdlib>
#include <cstring>
#include "helix/log.h"

using LogSink = void (*)(const char*, int, const char*);

static std::mutex g_log_mtx;
static std::vector<LogSink> g_sinks;
static std::deque<std::tuple<std::string,int,std::string>> g_queue; // pre-sink backlog while no sinks

static std::atomic<int> g_min_level{1}; // default INFO
static std::atomic<uint64_t> g_dispatched{0};
static std::atomic<uint64_t> g_dropped{0};
static std::atomic<uint64_t> g_dropped_overflow{0};
static std::atomic<uint64_t> g_dropped_filtered{0};
static size_t g_queue_cap = 256;

static void init_from_env_once() {
    static std::once_flag once;
    std::call_once(once, []{
        if (const char* cap = std::getenv("HELIX_LOG_QUEUE_CAP")) {
            long v = std::strtol(cap, nullptr, 10);
            if (v > 0) g_queue_cap = static_cast<size_t>(v);
        }
        if (const char* lvl = std::getenv("HELIX_LOG_MIN_LEVEL")) {
            long v = std::strtol(lvl, nullptr, 10);
            if (v >= 0 && v <= 3) g_min_level.store(static_cast<int>(v));
        }
    });
}

extern "C" void helix_log_register_sink(LogSink sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(g_log_mtx);
    if (std::find(g_sinks.begin(), g_sinks.end(), sink) == g_sinks.end()) {
        g_sinks.push_back(sink);
    }
    // On first sink registration, flush any queued messages
    if (!g_queue.empty()) {
        auto sinks_copy = g_sinks; // vector copy
        while (!g_queue.empty()) {
            auto [mod, lvl, msg] = std::move(g_queue.front());
            g_queue.pop_front();
            if (lvl < g_min_level.load()) { g_dropped_filtered++; continue; }
            for (auto& s : sinks_copy) s(mod.c_str(), lvl, msg.c_str());
            g_dispatched++;
        }
    }
}

extern "C" void helix_log_unregister_sink(LogSink sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(g_log_mtx);
    g_sinks.erase(std::remove(g_sinks.begin(), g_sinks.end(), sink), g_sinks.end());
}

extern "C" void helix_log_dispatch(const char* module_name, int level, const char* message) {
    init_from_env_once();
    const char* mod = module_name ? module_name : "(unknown)";
    const char* msg = message ? message : "";

    std::vector<LogSink> sinks_copy;
    {
        std::lock_guard<std::mutex> lock(g_log_mtx);
        sinks_copy = g_sinks;
        if (sinks_copy.empty()) {
            // Buffer until a sink becomes available
            if (g_queue.size() >= g_queue_cap) {
                g_dropped++; g_dropped_overflow++;
                return;
            }
            g_queue.emplace_back(std::string(mod), level, std::string(msg));
            return;
        }
    }

    // Have at least one sink; filter and dispatch directly
    if (level < g_min_level.load()) { g_dropped++; g_dropped_filtered++; return; }
    for (auto& s : sinks_copy) s(mod, level, msg);
    g_dispatched++;
}

extern "C" void helix_log_stats_get(struct HelixLogStats* out) {
    if (!out) return;
    init_from_env_once();
    std::lock_guard<std::mutex> lock(g_log_mtx);
    out->dispatched = g_dispatched.load();
    out->dropped = g_dropped.load();
    out->dropped_overflow = g_dropped_overflow.load();
    out->dropped_filtered = g_dropped_filtered.load();
    out->queued = g_queue.size();
    out->queue_capacity = g_queue_cap;
    out->sinks = g_sinks.size();
    out->min_level = g_min_level.load();
}

extern "C" void helix_log_min_level_set(int level) {
    if (level < 0) level = 0; if (level > 3) level = 3;
    g_min_level.store(level);
}

extern "C" int helix_log_min_level_get() {
    return g_min_level.load();
}
