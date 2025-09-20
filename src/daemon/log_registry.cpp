#include <vector>
#include <mutex>
#include <algorithm>

using LogSink = void (*)(const char*, int, const char*);

static std::mutex g_log_mtx;
static std::vector<LogSink> g_sinks;

extern "C" void helix_log_register_sink(LogSink sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(g_log_mtx);
    if (std::find(g_sinks.begin(), g_sinks.end(), sink) == g_sinks.end()) {
        g_sinks.push_back(sink);
    }
}

extern "C" void helix_log_unregister_sink(LogSink sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(g_log_mtx);
    g_sinks.erase(std::remove(g_sinks.begin(), g_sinks.end(), sink), g_sinks.end());
}

extern "C" void helix_log_dispatch(const char* module_name, int level, const char* message) {
    // Copy sinks under lock to minimize time holding the mutex
    std::vector<LogSink> sinks_copy;
    {
        std::lock_guard<std::mutex> lock(g_log_mtx);
        sinks_copy = g_sinks;
    }
    for (auto& s : sinks_copy) {
        s(module_name, level, message);
    }
}
