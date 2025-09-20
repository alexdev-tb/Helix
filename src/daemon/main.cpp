#include "helix/daemon.h"
#include "helix/version.h"
#include "ipc_server.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <string>

std::unique_ptr<helix::HelixDaemon> g_daemon;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_daemon) {
        g_daemon->shutdown();
        g_daemon.reset();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string RED = "\033[31m";
    const std::string CYAN = "\033[36m";
    const std::string BOLD = "\033[1m";
    const std::string RESET = "\033[0m";

    auto print_usage = [&]() {
        std::cout << "Helix Daemon (helixd)\n"
                  << "Usage: helixd [options] [modules_dir]\n\n"
                  << "Options:\n"
                  << "  -h, --help            Show this help and exit\n"
                  << "  --version             Show version and exit\n"
                  << "  --modules-dir <path>  Modules directory (defaults to ./modules)\n\n"
                  << "  --socket <path>       Unix socket path for control (default: /tmp/helixd.sock)\n"
                  << "  --foreground          Stay in foreground (do not daemonize)\n"
                  << "  --interactive         Run interactive CLI (legacy mode) on stdin/stdout\n\n"
                  << "If both --modules-dir and a positional modules_dir are provided,\n"
                  << "the explicit --modules-dir takes precedence." << std::endl;
    };

    auto print_version = [&]() {
#ifdef HELIX_CORE_VERSION
    std::cout << "Helix Daemon (helixd) version " << HELIX_CORE_VERSION << std::endl;
#else
    std::cout << "Helix Daemon (helixd) version (unknown)" << std::endl;
#endif
    };

    // Defaults
    std::string modules_dir = "./modules";
    std::string socket_path = "/tmp/helixd.sock";
    bool interactive = false;
    bool foreground = false;
    // Minimal argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "--modules-dir") {
            if (i + 1 < argc) {
                modules_dir = argv[++i];
            } else {
                std::cerr << RED << "Error: --modules-dir requires a <path>" << RESET << std::endl;
                print_usage();
                return 2;
            }
        } else if (arg == "--socket") {
            if (i + 1 < argc) { socket_path = argv[++i]; } else { std::cerr << RED << "Error: --socket requires <path>" << RESET << std::endl; return 2; }
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "--foreground") {
            foreground = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << RED << "Unknown option: " << arg << RESET << std::endl;
            print_usage();
            return 2;
        } else {
            // Positional modules_dir (kept for backward compatibility)
            modules_dir = arg;
        }
    }

    std::cout << BOLD << CYAN << "Starting Helix Daemon..." << RESET << std::endl;

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create daemon instance
    g_daemon = std::make_unique<helix::HelixDaemon>();

    // Initialize daemon
    if (!g_daemon->initialize(modules_dir)) {
        std::cerr << RED << "Failed to initialize Helix daemon" << RESET << std::endl;
        return 1;
    }

    std::cout << GREEN << "Helix daemon started successfully" << RESET << std::endl;
    std::cout << g_daemon->get_status() << std::endl;
    std::cout << YELLOW << "Type 'help' for commands. Press Ctrl+C to shutdown." << RESET << std::endl;

    // Launch mode: interactive CLI or IPC server
    if (!interactive) {
        std::cout << YELLOW << "Running in service mode. Control socket: " << socket_path << RESET << std::endl;
        // Command dispatcher for control requests
        auto handler = [&](const std::string& line) -> std::string {
            std::string cmd = line;
            auto trim = [](std::string& s){ while(!s.empty() && (s.back()=='\r' || s.back()==' ')) s.pop_back(); size_t p=0; while(p<s.size() && s[p]==' ') ++p; s.erase(0,p); };
            trim(cmd);
            if (cmd == "status") return g_daemon->get_status();
            if (cmd == "version") {
                std::string out;
#ifdef HELIX_CORE_VERSION
                out += std::string("core=") + std::string(HELIX_CORE_VERSION) + "\n";
#endif
#ifdef HELIX_API_VERSION
                out += std::string("api=") + std::string(HELIX_API_VERSION) + "\n";
#endif
                if (out.empty()) out = "ERR version unavailable\n";
                return out;
            }
            if (cmd == "list") {
                std::string out;
                for (auto& name : g_daemon->list_modules()) {
                    const auto* info = g_daemon->get_module_info(name);
                    std::string state = info ? helix::HelixDaemon::state_to_string(info->state) : std::string("Unknown");
                    out += name + " " + state + "\n";
                }
                if (out.empty()) out = "\n"; // send at least a newline to indicate success
                return out;
            }
            if (cmd.rfind("info ",0)==0) {
                auto name = cmd.substr(5);
                const auto* info = g_daemon->get_module_info(name);
                if (!info) return "ERR not installed";
                std::string out;
                out += "name=" + info->name + "\n";
                out += "version=" + info->version + "\n";
                out += "state=" + helix::HelixDaemon::state_to_string(info->state) + "\n";
                out += "description=" + info->manifest.description + "\n";
                out += "author=" + info->manifest.author + "\n";
                out += "license=" + info->manifest.license + "\n";
                out += "binary_path=" + info->manifest.binary_path + "\n";
                if (!info->manifest.minimum_core_version.empty()) out += "minimum_core_version=" + info->manifest.minimum_core_version + "\n";
                if (!info->manifest.minimum_api_version.empty()) out += "minimum_api_version=" + info->manifest.minimum_api_version + "\n";
                return out;
            }
            if (cmd.rfind("install ",0)==0) return g_daemon->install_module(cmd.substr(8)) ? "OK" : (std::string("ERR install: ") + g_daemon->last_error());
            if (cmd.rfind("enable ",0)==0) return g_daemon->enable_module(cmd.substr(7)) ? "OK" : (std::string("ERR enable: ") + g_daemon->last_error());
            if (cmd.rfind("start ",0)==0) return g_daemon->start_module(cmd.substr(6)) ? "OK" : (std::string("ERR start: ") + g_daemon->last_error());
            if (cmd.rfind("stop ",0)==0) return g_daemon->stop_module(cmd.substr(5)) ? "OK" : (std::string("ERR stop: ") + g_daemon->last_error());
            if (cmd.rfind("disable ",0)==0) return g_daemon->disable_module(cmd.substr(8)) ? "OK" : (std::string("ERR disable: ") + g_daemon->last_error());
            if (cmd.rfind("uninstall ",0)==0) return g_daemon->uninstall_module(cmd.substr(10)) ? "OK" : (std::string("ERR uninstall: ") + g_daemon->last_error());
            return std::string("ERR unknown command: ") + cmd;
        };

        helix::IpcServer server(socket_path);
        server.serve(handler);

        // After serve returns, proceed to shutdown
        std::cout << YELLOW << "Shutting down Helix daemon..." << RESET << std::endl;
        g_daemon->shutdown();
        g_daemon.reset();
        return 0;
    }

    // Interactive mode (legacy CLI)
    std::string command;
    auto prompt = [&]() {
        std::cout << BOLD << "helix> " << RESET;
        std::cout.flush();
    };
    prompt();
    while (std::getline(std::cin, command)) {
        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "status") {
            std::cout << g_daemon->get_status() << std::endl;
        } else if (command == "list") {
            auto modules = g_daemon->list_modules();
            std::cout << BOLD << "Installed modules:" << RESET << std::endl;
            for (const auto& module : modules) {
                auto info = g_daemon->get_module_info(module);
                if (info) {
                    std::string state = helix::HelixDaemon::state_to_string(info->state);
                    std::string color = (info->state == helix::ModuleState::RUNNING) ? GREEN : (info->state == helix::ModuleState::ERROR ? RED : CYAN);
                    std::cout << "  " << BOLD << module << RESET << " v" << info->version 
                              << " [" << color << state << RESET << "]" << std::endl;
                }
            }
        } else if (command.find("install ") == 0) {
            std::string package_path = command.substr(8);
            if (g_daemon->install_module(package_path)) {
                std::cout << GREEN << "Module installed successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to install module" << RESET << std::endl;
            }
        } else if (command.find("info ") == 0) {
            std::string module_name = command.substr(5);
            const auto* info = g_daemon->get_module_info(module_name);
            if (!info) {
                std::cout << RED << "Module '" << module_name << "' is not installed" << RESET << std::endl;
            } else {
                std::cout << BOLD << "Module info: " << info->name << RESET << std::endl;
                std::cout << "  Name:        " << info->name << std::endl;
                std::cout << "  Version:     " << info->version << std::endl;
                std::cout << "  Description: " << info->manifest.description << std::endl;
                std::cout << "  Author:      " << info->manifest.author << std::endl;
                std::cout << "  Binary:      " << info->manifest.binary_path << std::endl;
                if (!info->manifest.minimum_core_version.empty()) {
                    std::cout << "  Min Core:    " << info->manifest.minimum_core_version << std::endl;
                }
                std::cout << "  State:       " << helix::HelixDaemon::state_to_string(info->state) << std::endl;
            }
        } else if (command.find("enable ") == 0) {
            std::string module_name = command.substr(7);
            if (g_daemon->enable_module(module_name)) {
                std::cout << GREEN << "Module enabled successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to enable module" << RESET << std::endl;
            }
        } else if (command.find("start ") == 0) {
            std::string module_name = command.substr(6);
            if (g_daemon->start_module(module_name)) {
                std::cout << GREEN << "Module started successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to start module" << RESET << std::endl;
            }
        } else if (command.find("stop ") == 0) {
            std::string module_name = command.substr(5);
            if (g_daemon->stop_module(module_name)) {
                std::cout << GREEN << "Module stopped successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to stop module" << RESET << std::endl;
            }
        } else if (command.find("disable ") == 0) {
            std::string module_name = command.substr(8);
            if (g_daemon->disable_module(module_name)) {
                std::cout << GREEN << "Module disabled successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to disable module" << RESET << std::endl;
            }
        } else if (command.find("uninstall ") == 0) {
            std::string module_name = command.substr(10);
            if (g_daemon->uninstall_module(module_name)) {
                std::cout << GREEN << "Module uninstalled successfully" << RESET << std::endl;
            } else {
                std::cout << RED << "Failed to uninstall module" << RESET << std::endl;
            }
        } else if (command == "help") {
            std::cout << BOLD << "Available commands:" << RESET << std::endl;
            std::cout << "  status          - Show daemon status" << std::endl;
            std::cout << "  list            - List all modules" << std::endl;
            std::cout << "  info <name>     - Show module info (name, version, author, description)" << std::endl;
            std::cout << "  install <file.helx>  - Install module from a .helx package" << std::endl;
            std::cout << "  enable <name>   - Enable a module" << std::endl;
            std::cout << "  start <name>    - Start a module" << std::endl;
            std::cout << "  stop <name>     - Stop a running module" << std::endl;
            std::cout << "  disable <name>  - Disable (unload) a module" << std::endl;
            std::cout << "  uninstall <name>- Uninstall a module" << std::endl;
            std::cout << "  quit/exit       - Shutdown daemon" << std::endl;
        } else if (!command.empty()) {
            std::cout << RED << "Unknown command: " << command << RESET << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
        prompt();
    }

    // Graceful shutdown
    std::cout << YELLOW << "Shutting down Helix daemon..." << RESET << std::endl;
    g_daemon->shutdown();
    g_daemon.reset();

    return 0;
}