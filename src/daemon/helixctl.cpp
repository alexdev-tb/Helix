#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdlib>
// Do not include version macros here; helixctl queries the daemon for versions

namespace fs = std::filesystem;

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [--socket <path>] [--no-color] <command> [args...]\n"
              << "       " << prog << " install-service [--service-name helixd] [--modules-dir PATH] [--socket PATH] [--exec /path/to/helixd]\n\n"
              << "Commands:\n"
              << "  <command>            Send a single control command (status, list, info <name>, install <file.helx>, ...)\n"
              << "  install-service      Install and enable a systemd service for helixd (requires root)\n"
              << "  uninstall-service    Stop/disable and remove the helixd systemd service/socket (requires root)\n\n"
              << "Options:\n"
              << "  --socket <path>      Control socket path (defaults: $HELIX_SOCKET, /run/helixd/helixd.sock if exists, else /tmp/helixd.sock)\n"
              << "  --no-color           Disable ANSI colors in output\n"
              << "  --version            Query daemon and print Helix core and API versions\n";
}
static bool send_command(const std::string& socket_path, const std::string& command, std::string& response) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { response = std::string("socket: ") + std::strerror(errno); return false; }
    sockaddr_un addr{}; addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path.c_str());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        response = std::string("connect: ") + std::strerror(errno) + " (" + socket_path + ")";
        ::close(fd); return false;
    }
    std::string wire = command; if (wire.empty() || wire.back()!='\n') wire.push_back('\n');
    if (::write(fd, wire.data(), wire.size()) < 0) { response = std::string("write: ") + std::strerror(errno); ::close(fd); return false; }
    char buf[1024]; ssize_t n; response.clear();
    while ((n = ::read(fd, buf, sizeof(buf))) > 0) response.append(buf, buf + n);
    ::close(fd);
    return true;
}

static std::string detect_default_socket() {
    const char* env = std::getenv("HELIX_SOCKET");
    if (env && *env) return std::string(env);
    if (fs::exists("/run/helixd/helixd.sock")) return "/run/helixd/helixd.sock";
    return "/tmp/helixd.sock";
}

static std::string resolve_default_helixd_path(const char* /*argv0*/) {
    // Try sibling of helixctl by reading /proc/self/exe without PATH_MAX
    std::vector<char> buf(1024);
    ssize_t len = -1;
    for (;;) {
        len = ::readlink("/proc/self/exe", buf.data(), buf.size());
        if (len < 0) break;
        if (static_cast<size_t>(len) < buf.size()) break; // buffer too small, try again
        buf.resize(buf.size() * 2);
    }
    if (len > 0) {
        fs::path p(std::string(buf.data(), static_cast<size_t>(len)));
        fs::path sibling = p.parent_path() / "helixd";
        std::error_code ec;
        if (fs::exists(sibling, ec) && fs::is_regular_file(sibling, ec)) return sibling.string();
    }
    // Fallback to PATH lookup by name
    return "helixd";
}

static int install_service(const std::string& service_name,
                           const std::string& exec_path,
                           const std::string& modules_dir,
                           const std::string& socket_path) {
    std::string unit_path = "/etc/systemd/system/" + service_name + ".service";
    std::string unit =
        "[Unit]\n"
        "Description=Helix Daemon\n"
        "After=network.target\n\n"
        "[Service]\n"
        "Type=simple\n"
        "ExecStart=" + exec_path + " --modules-dir " + modules_dir + " --socket " + socket_path + " --foreground\n"
        "RuntimeDirectory=helix\n"
        "RuntimeDirectoryMode=0755\n"
        "Restart=on-failure\n"
        "RestartSec=2s\n\n"
        "[Install]\n"
        "WantedBy=multi-user.target\n";

    // Optional socket unit for socket activation
    std::string socket_unit_path = "/etc/systemd/system/" + service_name + ".socket";
    std::string socket_unit =
        "[Unit]\n"
        "Description=Helix Daemon Socket\n\n"
        "[Socket]\n"
        "ListenStream=" + socket_path + "\n"
        "SocketMode=0666\n"
        "DirectoryMode=0755\n\n"
        "[Install]\n"
        "WantedBy=sockets.target\n";

    // Write unit file
    std::error_code ec;
    fs::create_directories("/etc/systemd/system", ec);
    std::ofstream ofs(unit_path);
    if (!ofs) {
        std::cerr << "Failed to write " << unit_path << ". Are you root?" << std::endl;
        return 1;
    }
    ofs << unit;
    ofs.close();

    // Write socket unit (best effort)
    {
        std::ofstream sofs(socket_unit_path);
        if (sofs) { sofs << socket_unit; sofs.close(); }
    }

    // Reload systemd
    int r1 = std::system("systemctl daemon-reload");
    if (r1 != 0) {
        std::cerr << "systemctl daemon-reload failed (code " << r1 << ")" << std::endl;
        return 1;
    }

    // Enable and start socket first (activates the service on demand)
    std::string cmd = "systemctl enable --now " + service_name + ".socket";
    int r2 = std::system(cmd.c_str());
    if (r2 != 0) {
        std::cerr << "systemctl enable --now (socket) failed (code " << r2 << ")" << std::endl;
        // continue to enable service directly as fallback
        std::string cmd2 = "systemctl enable --now " + service_name;
        int r3 = std::system(cmd2.c_str());
        if (r3 != 0) {
            std::cerr << "systemctl enable --now (service) failed (code " << r3 << ")" << std::endl;
            return 1;
        }
    }

    // Always ensure the service itself is enabled as well
    {
        std::string svcEnable = "systemctl enable " + service_name + ".service";
        (void)std::system(svcEnable.c_str());
    }

    // Additionally, explicitly enable the socket and start the service now
    // (even if socket activation is used) so modules are loaded immediately.
    {
        std::string sockEnable = "systemctl enable " + service_name + ".socket";
        (void)std::system(sockEnable.c_str());
        std::string svcStart = "systemctl start " + service_name + ".service";
        (void)std::system(svcStart.c_str());
    }

    std::cout << "Installed and started service/socket for '" << service_name << "'\n";
    std::cout << "Unit: " << unit_path << "\n";
    std::cout << "Socket Unit: " << socket_unit_path << "\n";
    std::cout << "Socket: " << socket_path << "\n";
    return 0;
}

static int uninstall_service(const std::string& service_name) {
    std::string unit_path = "/etc/systemd/system/" + service_name + ".service";
    std::string socket_unit_path = "/etc/systemd/system/" + service_name + ".socket";

    // Stop units if active (best-effort)
    {
        std::string stopSocket = "systemctl stop " + service_name + ".socket";
        (void)std::system(stopSocket.c_str());
        std::string stopService = "systemctl stop " + service_name + ".service";
        (void)std::system(stopService.c_str());
    }

    // Disable units (best-effort)
    {
        std::string disableSocket = "systemctl disable " + service_name + ".socket";
        (void)std::system(disableSocket.c_str());
        std::string disableService = "systemctl disable " + service_name + ".service";
        (void)std::system(disableService.c_str());
    }

    // Remove unit files
    std::error_code ec;
    bool removed_any = false;
    if (fs::exists(socket_unit_path, ec)) { fs::remove(socket_unit_path, ec); removed_any = true; }
    if (fs::exists(unit_path, ec)) { fs::remove(unit_path, ec); removed_any = true; }

    // Reload systemd to pick up changes
    int r = std::system("systemctl daemon-reload");
    if (r != 0) {
        std::cerr << "systemctl daemon-reload failed (code " << r << ")" << std::endl;
        // continue
    }

    if (removed_any) {
        std::cout << "Uninstalled service/socket for '" << service_name << "'\n";
        std::cout << "Removed: " << unit_path << " (if existed)\n";
        std::cout << "Removed: " << socket_unit_path << " (if existed)\n";
    } else {
        std::cout << "No unit files found for '" << service_name << "'" << std::endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    std::string socket_path = detect_default_socket();
    bool no_color = false;
    int i = 1;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { print_usage(argv[0]); return 0; }
        if (arg == "--version") {
            std::string resp;
            if (!send_command(socket_path, "version", resp)) {
                std::cerr << resp << std::endl; return 1;
            }
            // Expect lines core=X, api=Y
            std::istringstream iss(resp);
            std::string line, core, api;
            while (std::getline(iss, line)) {
                if (line.rfind("core=",0)==0) core = line.substr(5);
                else if (line.rfind("api=",0)==0) api = line.substr(4);
            }
            if (!core.empty()) std::cout << "Helix core: " << core << "\n";
            if (!api.empty()) std::cout << "Helix API:  " << api << "\n";
            if (core.empty() && api.empty()) std::cout << resp;
            return 0;
        }
        if (arg == "--socket") {
            if (i + 1 < argc) { socket_path = argv[++i]; } else { print_usage(argv[0]); return 2; }
        } else if (arg == "--no-color") {
            no_color = true;
        } else {
            break;
        }
    }
    if (i >= argc) { print_usage(argv[0]); return 2; }

    std::string sub = argv[i++];
    if (sub == "install-service") {
    std::string service_name = "helixd";
    std::string modules_dir = "/var/lib/helix/modules";
        std::string exec_path = resolve_default_helixd_path(argv[0]);
    if (socket_path == "/tmp/helixd.sock") socket_path = "/run/helixd/helixd.sock";
        // Parse flags for install-service
        while (i < argc) {
            std::string a = argv[i++];
            if (a == "--service-name" && i < argc) service_name = argv[i++];
            else if (a == "--modules-dir" && i < argc) modules_dir = argv[i++];
            else if (a == "--socket" && i < argc) socket_path = argv[i++];
            else if (a == "--exec" && i < argc) exec_path = argv[i++];
            else { std::cerr << "Unknown option for install-service: " << a << std::endl; return 2; }
        }
        return install_service(service_name, exec_path, modules_dir, socket_path);
    }

    if (sub == "uninstall-service") {
        std::string service_name = "helixd";
        // Parse flags for uninstall-service
        while (i < argc) {
            std::string a = argv[i++];
            if (a == "--service-name" && i < argc) service_name = argv[i++];
            else { std::cerr << "Unknown option for uninstall-service: " << a << std::endl; return 2; }
        }
        return uninstall_service(service_name);
    }

    // Default behavior: send command to daemon
    // Special-case: normalize install path to absolute (daemon may have different CWD)
    std::string cmd;
    {
        int start = i - 1; // include 'sub' as first token
        for (int j = start; j < argc; ++j) {
            if (j > start) cmd.push_back(' ');
            std::string token = argv[j];
            if (j == start && token == "install" && (j + 1) < argc) {
                cmd += token;
                cmd.push_back(' ');
                fs::path p(argv[j+1]);
                std::error_code ec;
                fs::path abs = fs::absolute(p, ec);
                cmd += abs.string();
                ++j; // consumed the path
            } else {
                cmd += token;
            }
        }
        cmd.push_back('\n');
    }

    std::string resp;
    if (!send_command(socket_path, cmd, resp)) { std::cerr << resp << std::endl; return 1; }
    // Pretty-print for known commands
    auto color = [&](const std::string& s, const char* code){ return no_color ? s : (std::string("\033[") + code + "m" + s + "\033[0m"); };
    auto bold = [&](const std::string& s){ return color(s, "1"); };
    auto green = [&](const std::string& s){ return color(s, "32"); };
    auto yellow = [&](const std::string& s){ return color(s, "33"); };
    auto red = [&](const std::string& s){ return color(s, "31"); };

    if (sub == "list") {
        // format: name state
        std::istringstream iss(resp);
        std::string line; bool any=false;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            any=true;
            auto pos = line.find(' ');
            std::string name = pos==std::string::npos ? line : line.substr(0,pos);
            std::string state = pos==std::string::npos ? std::string("") : line.substr(pos+1);
            std::string colored = state == "Running" ? green(state) : (state == "Error" ? red(state) : yellow(state));
            std::cout << bold(name) << " [" << colored << "]\n";
        }
        if (!any) std::cout << "(no modules)\n";
        return 0;
    }

    if (sub == "info" && i-1 < argc) {
        // Key=Value lines; color keys and highlight state
        std::istringstream iss(resp);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.rfind("state=",0)==0) {
                auto val = line.substr(6);
                std::string colored = val == "Running" ? green(val) : (val == "Error" ? red(val) : yellow(val));
                std::cout << bold("state") << "=" << colored << "\n";
            } else if (!line.empty()) {
                auto pos = line.find('=');
                if (pos==std::string::npos) std::cout << line << "\n";
                else std::cout << bold(line.substr(0,pos)) << "=" << line.substr(pos+1) << "\n";
            }
        }
        return 0;
    }

    // Default: print as-is (may include ERR ...)
    std::cout << resp;
    return 0;
}
