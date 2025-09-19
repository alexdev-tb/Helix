#include "helix/daemon.h"
#include <iostream>
#include <csignal>
#include <memory>

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
    std::cout << "Starting Helix Daemon..." << std::endl;

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create daemon instance
    g_daemon = std::make_unique<helix::HelixDaemon>();

    // Default modules directory
    std::string modules_dir = "./modules";
    if (argc > 1) {
        modules_dir = argv[1];
    }

    // Initialize daemon
    if (!g_daemon->initialize(modules_dir)) {
        std::cerr << "Failed to initialize Helix daemon" << std::endl;
        return 1;
    }

    std::cout << "Helix daemon started successfully" << std::endl;
    std::cout << g_daemon->get_status() << std::endl;
    std::cout << "Press Ctrl+C to shutdown" << std::endl;

    // Simple command loop for demonstration
    std::string command;
    while (std::getline(std::cin, command)) {
        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "status") {
            std::cout << g_daemon->get_status() << std::endl;
        } else if (command == "list") {
            auto modules = g_daemon->list_modules();
            std::cout << "Installed modules:" << std::endl;
            for (const auto& module : modules) {
                auto info = g_daemon->get_module_info(module);
                if (info) {
                    std::cout << "  " << module << " v" << info->version 
                              << " [" << helix::HelixDaemon::state_to_string(info->state) << "]" << std::endl;
                }
            }
        } else if (command.find("install ") == 0) {
            std::string package_path = command.substr(8);
            if (g_daemon->install_module(package_path)) {
                std::cout << "Module installed successfully" << std::endl;
            } else {
                std::cout << "Failed to install module" << std::endl;
            }
        } else if (command.find("enable ") == 0) {
            std::string module_name = command.substr(7);
            if (g_daemon->enable_module(module_name)) {
                std::cout << "Module enabled successfully" << std::endl;
            } else {
                std::cout << "Failed to enable module" << std::endl;
            }
        } else if (command.find("start ") == 0) {
            std::string module_name = command.substr(6);
            if (g_daemon->start_module(module_name)) {
                std::cout << "Module started successfully" << std::endl;
            } else {
                std::cout << "Failed to start module" << std::endl;
            }
        } else if (command.find("stop ") == 0) {
            std::string module_name = command.substr(5);
            if (g_daemon->stop_module(module_name)) {
                std::cout << "Module stopped successfully" << std::endl;
            } else {
                std::cout << "Failed to stop module" << std::endl;
            }
        } else if (command.find("disable ") == 0) {
            std::string module_name = command.substr(8);
            if (g_daemon->disable_module(module_name)) {
                std::cout << "Module disabled successfully" << std::endl;
            } else {
                std::cout << "Failed to disable module" << std::endl;
            }
        } else if (command.find("uninstall ") == 0) {
            std::string module_name = command.substr(10);
            if (g_daemon->uninstall_module(module_name)) {
                std::cout << "Module uninstalled successfully" << std::endl;
            } else {
                std::cout << "Failed to uninstall module" << std::endl;
            }
        } else if (command == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "  status          - Show daemon status" << std::endl;
            std::cout << "  list            - List all modules" << std::endl;
            std::cout << "  install <path>  - Install module from path" << std::endl;
            std::cout << "  enable <name>   - Enable a module" << std::endl;
            std::cout << "  start <name>    - Start a module" << std::endl;
            std::cout << "  stop <name>     - Stop a running module" << std::endl;
            std::cout << "  disable <name>  - Disable (unload) a module" << std::endl;
            std::cout << "  uninstall <name>- Uninstall a module" << std::endl;
            std::cout << "  quit/exit       - Shutdown daemon" << std::endl;
        } else if (!command.empty()) {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
    }

    // Graceful shutdown
    std::cout << "Shutting down Helix daemon..." << std::endl;
    g_daemon->shutdown();
    g_daemon.reset();

    return 0;
}