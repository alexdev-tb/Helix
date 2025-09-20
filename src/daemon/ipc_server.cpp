#include "ipc_server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;
namespace helix {

IpcServer::IpcServer(std::string socket_path) : socket_path_(std::move(socket_path)) {}

IpcServer::~IpcServer() { stop(); }

void IpcServer::stop() {
    running_.store(false);
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    // Cleanup stale socket file
    if (created_socket_ && !socket_path_.empty() && fs::exists(socket_path_)) {
        fs::remove(socket_path_);
    }
}

bool IpcServer::serve(Handler handler) {
    // Ensure parent directory exists
    try {
        auto dir = fs::path(socket_path_).parent_path();
        if (!dir.empty()) fs::create_directories(dir);
    } catch (...) {
        std::cerr << "IPC: failed to create socket directory" << std::endl;
        return false;
    }

    // systemd socket activation: LISTEN_FDS=1, fd starts at 3
    int sd_fds = 0;
    const char* env_pid = std::getenv("LISTEN_PID");
    const char* env_fds = std::getenv("LISTEN_FDS");
    if (env_pid && env_fds) {
        pid_t pid = static_cast<pid_t>(std::atoi(env_pid));
        if (pid == getpid()) {
            sd_fds = std::atoi(env_fds);
        }
    }

    if (sd_fds > 0) {
        listen_fd_ = 3; // first passed fd
        created_socket_ = false;
    } else {
        // Remove any stale socket we control
        if (fs::exists(socket_path_)) {
            fs::remove(socket_path_);
        }

        listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            std::cerr << "IPC: socket() failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "IPC: bind() failed: " << std::strerror(errno) << std::endl;
            stop();
            return false;
        }

        // Relax permissions to allow non-root clients by default (0666)
        if (::chmod(socket_path_.c_str(), 0666) < 0) {
            std::cerr << "IPC: chmod(" << socket_path_ << ") failed: " << std::strerror(errno) << std::endl;
        }

        if (::listen(listen_fd_, 4) < 0) {
            std::cerr << "IPC: listen() failed: " << std::strerror(errno) << std::endl;
            stop();
            return false;
        }
        created_socket_ = true;
    }

    running_.store(true);
    while (running_.load()) {
        int client_fd = ::accept(listen_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!running_.load()) break;
            std::cerr << "IPC: accept() failed: " << std::strerror(errno) << std::endl;
            break;
        }

        // Read a single line command
        std::string input;
        char buf[1024];
        ssize_t n;
        while ((n = ::read(client_fd, buf, sizeof(buf))) > 0) {
            input.append(buf, buf + n);
            if (input.find('\n') != std::string::npos) break;
        }
        if (!input.empty() && input.back() == '\n') input.pop_back();

        std::string response;
        try {
            response = handler ? handler(input) : std::string("ERR no handler\n");
        } catch (...) {
            response = "ERR exception\n";
        }
        if (!response.empty() && response.back() != '\n') response.push_back('\n');
        ::write(client_fd, response.data(), response.size());
        ::close(client_fd);
    }

    stop();
    return true;
}

} // namespace helix
