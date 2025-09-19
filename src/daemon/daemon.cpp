#include "helix/daemon.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#ifdef __unix__
#include <unistd.h>
#endif

namespace helix {

HelixDaemon::HelixDaemon() 
    : modules_directory_(""), 
      module_loader_(std::make_unique<ModuleLoader>()),
      dependency_resolver_(std::make_unique<DependencyResolver>()),
      initialized_(false) {
}

HelixDaemon::~HelixDaemon() {
    if (initialized_) {
        shutdown();
    }
}

bool HelixDaemon::initialize(const std::string& modules_directory) {
    if (initialized_) {
        std::cerr << "Daemon is already initialized" << std::endl;
        return false;
    }

    modules_directory_ = modules_directory;

    // Create modules directory if it doesn't exist
    try {
        std::filesystem::create_directories(modules_directory_);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create modules directory: " << e.what() << std::endl;
        return false;
    }

    // Scan for existing modules
    if (!scan_modules_directory()) {
        std::cerr << "Failed to scan modules directory" << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "Helix daemon initialized with modules directory: " << modules_directory_ << std::endl;
    return true;
}

void HelixDaemon::shutdown() {
    if (!initialized_) {
        return;
    }

    std::cout << "Shutting down Helix daemon..." << std::endl;

    // Stop all running modules
    for (auto& [name, info] : module_registry_) {
        if (info.state == ModuleState::RUNNING) {
            std::cout << "Stopping module: " << name << std::endl;
            stop_module(name);
        }
    }

    // Disable all enabled modules
    for (auto& [name, info] : module_registry_) {
        if (info.state == ModuleState::INITIALIZED || info.state == ModuleState::STOPPED) {
            std::cout << "Disabling module: " << name << std::endl;
            disable_module(name);
        }
    }

    module_registry_.clear();
    dependency_resolver_->clear();
    initialized_ = false;
    
    std::cout << "Helix daemon shutdown complete" << std::endl;
}

bool HelixDaemon::install_module(const std::string& package_path) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    // Support two inputs:
    // 1) A .helx archive (tar.gz) produced by helxcompiler
    // 2) A directory containing manifest.json and binary
    std::cout << "Installing module from: " << package_path << std::endl;

    std::string source_dir = package_path;
    ModuleManifest manifest;

    // If it's a .helx file, extract it to a temp dir to read the manifest
    if (std::filesystem::is_regular_file(package_path)) {
        auto ext = std::filesystem::path(package_path).extension().string();
        if (ext == ".helx") {
            // Create temp dir
            long suffix = 0;
#ifdef __unix__
            suffix = static_cast<long>(::getpid());
#else
            suffix = static_cast<long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
#endif
            std::string temp_dir = modules_directory_ + "/.tmp_install_" + std::to_string(suffix);
            try { std::filesystem::create_directories(temp_dir); } catch (...) {}

            // Extract archive using system tar for now
            std::stringstream cmd;
            cmd << "tar -xzf \"" << package_path << "\" -C \"" << temp_dir << "\"";
            int rc = std::system(cmd.str().c_str());
            if (rc != 0) {
                std::cerr << "Failed to extract .helx package: exit code " << rc << std::endl;
                std::filesystem::remove_all(temp_dir);
                return false;
            }
            source_dir = temp_dir;

            // Parse manifest from extracted temp
            if (!load_module_manifest(source_dir, manifest)) {
                std::cerr << "Failed to load manifest from extracted package" << std::endl;
                std::filesystem::remove_all(temp_dir);
                return false;
            }

            // Move extracted contents into final module install location (by name)
            std::string module_path = extract_package(source_dir, manifest.name);
            std::filesystem::remove_all(source_dir); // cleanup temp

            if (module_path.empty()) {
                std::cerr << "Failed to install extracted package" << std::endl;
                return false;
            }

            // Register module
            DaemonModuleInfo module_info;
            module_info.name = manifest.name;
            module_info.version = manifest.version;
            module_info.install_path = module_path;
            module_info.manifest = manifest;
            module_info.state = ModuleState::INSTALLED;

            module_registry_[manifest.name] = module_info;
            dependency_resolver_->add_module(manifest);

            std::cout << "Successfully installed module: " << manifest.name << " v" << manifest.version << std::endl;
            return true;
        }
    }

    // Otherwise treat as a directory package
    if (!load_module_manifest(source_dir, manifest)) {
        std::cerr << "Failed to load manifest from package" << std::endl;
        return false;
    }

    // Check if module is already installed
    if (module_registry_.find(manifest.name) != module_registry_.end()) {
        std::cerr << "Module '" << manifest.name << "' is already installed" << std::endl;
        return false;
    }

    // Extract/copy package dir to modules directory
    std::string module_path = extract_package(source_dir, manifest.name);
    if (module_path.empty()) {
        std::cerr << "Failed to extract package" << std::endl;
        return false;
    }

    // Create module info
    DaemonModuleInfo module_info;
    module_info.name = manifest.name;
    module_info.version = manifest.version;
    module_info.install_path = module_path;
    module_info.manifest = manifest;
    module_info.state = ModuleState::INSTALLED;

    // Add to registry and dependency resolver
    module_registry_[manifest.name] = module_info;
    dependency_resolver_->add_module(manifest);

    std::cout << "Successfully installed module: " << manifest.name << " v" << manifest.version << std::endl;
    return true;
}

bool HelixDaemon::uninstall_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        return false;
    }

    // Check if any other modules depend on this one
    auto dependents = dependency_resolver_->get_dependents(module_name);
    if (!dependents.empty()) {
        std::cerr << "Cannot uninstall '" << module_name << "': required by ";
        for (size_t i = 0; i < dependents.size(); ++i) {
            std::cerr << dependents[i];
            if (i < dependents.size() - 1) std::cerr << ", ";
        }
        std::cerr << std::endl;
        return false;
    }

    // Disable module if it's enabled
    if (it->second.state != ModuleState::INSTALLED) {
        if (!disable_module(module_name)) {
            std::cerr << "Failed to disable module before uninstallation" << std::endl;
            return false;
        }
    }

    // Remove module files
    if (!remove_module_files(module_name)) {
        std::cerr << "Failed to remove module files" << std::endl;
        return false;
    }

    // Remove from registry and dependency resolver
    dependency_resolver_->remove_module(module_name);
    module_registry_.erase(it);

    std::cout << "Successfully uninstalled module: " << module_name << std::endl;
    return true;
}

bool HelixDaemon::enable_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::INSTALLED) {
        std::cerr << "Module '" << module_name << "' is already enabled" << std::endl;
        return false;
    }

    // Resolve and load dependencies first
    if (!resolve_and_load_dependencies(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to resolve dependencies");
        return false;
    }

    // Load the module
    std::string binary_path = module_info.install_path + "/" + module_info.manifest.binary_path;
    if (!module_loader_->load_module(binary_path, module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to load module binary");
        return false;
    }

    update_module_state(module_name, ModuleState::LOADED);

    // Initialize the module
    if (!module_loader_->initialize_module(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to initialize module");
        return false;
    }

    update_module_state(module_name, ModuleState::INITIALIZED);
    std::cout << "Successfully enabled module: " << module_name << std::endl;
    return true;
}

bool HelixDaemon::disable_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state == ModuleState::INSTALLED) {
        std::cerr << "Module '" << module_name << "' is already disabled" << std::endl;
        return false;
    }

    // Stop module if it's running
    if (module_info.state == ModuleState::RUNNING) {
        if (!stop_module(module_name)) {
            return false;
        }
    }

    // Unload the module
    if (!module_loader_->unload_module(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to unload module");
        return false;
    }

    update_module_state(module_name, ModuleState::INSTALLED);
    std::cout << "Successfully disabled module: " << module_name << std::endl;
    return true;
}

bool HelixDaemon::start_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::INITIALIZED && module_info.state != ModuleState::STOPPED) {
        std::cerr << "Module '" << module_name << "' must be enabled before starting" << std::endl;
        return false;
    }

    if (!module_loader_->start_module(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to start module");
        return false;
    }

    update_module_state(module_name, ModuleState::RUNNING);
    std::cout << "Successfully started module: " << module_name << std::endl;
    return true;
}

bool HelixDaemon::stop_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::RUNNING) {
        std::cerr << "Module '" << module_name << "' is not running" << std::endl;
        return false;
    }

    if (!module_loader_->stop_module(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to stop module");
        return false;
    }

    update_module_state(module_name, ModuleState::STOPPED);
    std::cout << "Successfully stopped module: " << module_name << std::endl;
    return true;
}

const DaemonModuleInfo* HelixDaemon::get_module_info(const std::string& module_name) const {
    auto it = module_registry_.find(module_name);
    return (it != module_registry_.end()) ? &it->second : nullptr;
}

std::vector<std::string> HelixDaemon::list_modules() const {
    std::vector<std::string> modules;
    modules.reserve(module_registry_.size());
    
    for (const auto& [name, info] : module_registry_) {
        modules.push_back(name);
    }
    
    return modules;
}

std::vector<std::string> HelixDaemon::list_modules_by_state(ModuleState state) const {
    std::vector<std::string> modules;
    
    for (const auto& [name, info] : module_registry_) {
        if (info.state == state) {
            modules.push_back(name);
        }
    }
    
    return modules;
}

bool HelixDaemon::refresh_modules() {
    if (!initialized_) {
        return false;
    }
    
    return scan_modules_directory();
}

std::string HelixDaemon::get_status() const {
    std::stringstream status;
    status << "Helix Daemon Status:\n";
    status << "  Initialized: " << (initialized_ ? "Yes" : "No") << "\n";
    status << "  Modules Directory: " << modules_directory_ << "\n";
    status << "  Total Modules: " << module_registry_.size() << "\n";
    
    for (auto state : {ModuleState::INSTALLED, ModuleState::LOADED, ModuleState::INITIALIZED, 
                       ModuleState::RUNNING, ModuleState::STOPPED, ModuleState::ERROR}) {
        auto modules = list_modules_by_state(state);
        if (!modules.empty()) {
            status << "  " << state_to_string(state) << ": " << modules.size() << "\n";
        }
    }
    
    return status.str();
}

bool HelixDaemon::scan_modules_directory() {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(modules_directory_)) {
            if (entry.is_directory()) {
                ModuleManifest manifest;
                if (load_module_manifest(entry.path().string(), manifest)) {
                    DaemonModuleInfo module_info;
                    module_info.name = manifest.name;
                    module_info.version = manifest.version;
                    module_info.install_path = entry.path().string();
                    module_info.manifest = manifest;
                    module_info.state = ModuleState::INSTALLED;

                    module_registry_[manifest.name] = module_info;
                    dependency_resolver_->add_module(manifest);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning modules directory: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool HelixDaemon::load_module_manifest(const std::string& module_path, ModuleManifest& manifest) {
    std::string manifest_path = module_path + "/manifest.json";
    
    ManifestParser parser;
    if (!parser.parse_from_file(manifest_path, manifest)) {
        std::cerr << "Failed to parse manifest at " << manifest_path << ": " 
                  << parser.get_last_error() << std::endl;
        return false;
    }
    
    return true;
}

std::string HelixDaemon::extract_package(const std::string& package_path, const std::string& module_name) {
    // Simple implementation: assume package_path is already a directory
    // In a real implementation, this would extract a .helx archive
    std::string destination = modules_directory_ + "/" + module_name;
    
    try {
        if (package_path != destination) {
            std::filesystem::copy(package_path, destination, 
                                std::filesystem::copy_options::recursive);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to extract package: " << e.what() << std::endl;
        return "";
    }
    
    return destination;
}

bool HelixDaemon::remove_module_files(const std::string& module_name) {
    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        return false;
    }
    
    try {
        std::filesystem::remove_all(it->second.install_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to remove module files: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

void HelixDaemon::update_module_state(const std::string& module_name, ModuleState new_state, 
                                     const std::string& error_message) {
    auto it = module_registry_.find(module_name);
    if (it != module_registry_.end()) {
        it->second.state = new_state;
        it->second.error_message = error_message;
    }
}

bool HelixDaemon::resolve_and_load_dependencies(const std::string& module_name) {
    auto result = dependency_resolver_->resolve_dependencies({module_name});
    
    if (!result.success) {
        std::cerr << "Failed to resolve dependencies for " << module_name << std::endl;
        if (!result.missing_deps.empty()) {
            std::cerr << "Missing dependencies: ";
            for (const auto& dep : result.missing_deps) {
                std::cerr << dep << " ";
            }
            std::cerr << std::endl;
        }
        if (!result.circular_deps.empty()) {
            std::cerr << "Circular dependencies: ";
            for (const auto& dep : result.circular_deps) {
                std::cerr << dep << " ";
            }
            std::cerr << std::endl;
        }
        return false;
    }

    // Load dependencies in order (excluding the target module itself)
    for (const auto& dep_name : result.load_order) {
        if (dep_name == module_name) {
            continue; // Skip the target module itself
        }
        
        auto dep_it = module_registry_.find(dep_name);
        if (dep_it != module_registry_.end() && dep_it->second.state == ModuleState::INSTALLED) {
            if (!enable_module(dep_name)) {
                return false;
            }
        }
    }

    return true;
}

std::string HelixDaemon::state_to_string(ModuleState state) {
    switch (state) {
        case ModuleState::UNKNOWN: return "Unknown";
        case ModuleState::INSTALLED: return "Installed";
        case ModuleState::LOADED: return "Loaded";
        case ModuleState::INITIALIZED: return "Initialized";
        case ModuleState::RUNNING: return "Running";
        case ModuleState::STOPPED: return "Stopped";
        case ModuleState::ERROR: return "Error";
        default: return "Invalid";
    }
}

} // namespace helix