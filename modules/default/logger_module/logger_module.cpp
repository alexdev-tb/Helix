// Default logger module: registers a single stdout/stderr sink
#include "helix/module.h"
#include "helix/log.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <atomic>

namespace {

// Synchronize writes to avoid interleaved lines across threads
static std::mutex g_io_mtx;
static std::atomic<bool> g_registered{false};

inline const char* level_to_str(int level) noexcept {
    switch (level) {
        case 0: return "DEBUG";
        case 1: return "INFO";
        case 2: return "WARN";
        case 3: return "ERROR";
        default: return "INFO";
    }
}

// Format timestamp with milliseconds in local time, thread-safe
inline void format_timestamp(char* buf, size_t len) noexcept {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto secs = time_point_cast<seconds>(now);
    std::time_t t = system_clock::to_time_t(secs);
    auto ms = duration_cast<milliseconds>(now - secs).count();
#if defined(__unix__)
    std::tm tm{};
    localtime_r(&t, &tm);
#else
    // Fallback with potential thread-safety caveats on non-POSIX
    std::tm tm = *std::localtime(&t);
#endif
    // Format: YYYY-MM-DD HH:MM:SS.mmm
    std::snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms));
}

static void logger_sink_stdout(const char* module_name, int level, const char* message) {
    char ts[40];
    format_timestamp(ts, sizeof(ts));
    const char* lvl = level_to_str(level);
    const char* mod = module_name ? module_name : "(unknown)";
    const char* msg = message ? message : "";
    std::lock_guard<std::mutex> lock(g_io_mtx);
    std::ostream& os = (level >= 3) ? std::cerr : std::cout;
    os << "[" << ts << "] [" << mod << "] [" << lvl << "] " << msg << std::endl;
}

} // namespace

extern "C" {

int helix_module_init() {
    helix_log("ConsoleLogger", "Logger module initialized", HELIX_LOG_INFO);
    return 0;
}

int helix_module_start() {
    if (!g_registered.load(std::memory_order_acquire)) {
        if (auto reg = helix_log_get_register()) {
            reg(&logger_sink_stdout);
            g_registered.store(true, std::memory_order_release);
        }
    }
    helix_log("ConsoleLogger", "Logger sink registered", HELIX_LOG_INFO);
    return 0;
}

int helix_module_stop() {
    if (g_registered.load(std::memory_order_acquire)) {
        if (auto unreg = helix_log_get_unregister()) {
            unreg(&logger_sink_stdout);
            g_registered.store(false, std::memory_order_release);
        }
    }
    helix_log("ConsoleLogger", "Logger sink unregistered", HELIX_LOG_INFO);
    return 0;
}

void helix_module_destroy() {
    helix_log("ConsoleLogger", "Logger module destroyed", HELIX_LOG_DEBUG);
}

}
