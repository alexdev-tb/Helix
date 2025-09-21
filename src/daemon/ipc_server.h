#ifndef HELIX_IPC_SERVER_H
#define HELIX_IPC_SERVER_H

#include <atomic>
#include <functional>
#include <string>

namespace helix {

class IpcServer {
public:
    using Handler = std::function<std::string(const std::string&)>;

    explicit IpcServer(std::string socket_path);
    ~IpcServer();

    // Start listening and handling connections in the current thread.
    // Returns true on clean shutdown, false on fatal error.
    bool serve(Handler handler);

    void stop();

    const std::string& socket_path() const { return socket_path_; }

private:
    std::string socket_path_;
    std::atomic<bool> running_{false};
    int listen_fd_{-1};
    bool created_socket_{false};
};

} // namespace helix

#endif // HELIX_IPC_SERVER_H
