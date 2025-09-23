#include "helix/daemon.h"
#include "helix/version.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <regex>
#include <algorithm>
#include <set>
#ifdef __unix__
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif


namespace helix {

static int run_program(const std::vector<std::string>& args) {
    if (args.empty()) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        std::vector<char*> argv; argv.reserve(args.size() + 1);
        for (const auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

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

    // Attempt to restore previously saved module states (best-effort)
    try {
        std::unordered_map<std::string, ModuleState> saved;
        if (load_saved_module_states(saved)) {
            if (!saved.empty()) {
                std::cout << "Loaded saved module states from '" << state_file_path() << "' (" << saved.size() << ")" << std::endl;
            } else {
                std::cout << "No saved module state to restore (" << state_file_path() << ")" << std::endl;
            }
            restore_saved_states(saved);
        } else {
            std::cerr << "Failed to load saved module states from '" << state_file_path() << "'" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "State restore failed: " << e.what() << std::endl;
    }

    std::cout << "Helix daemon initialized with modules directory: " << modules_directory_ << std::endl;
    return true;
}

void HelixDaemon::shutdown() {
    if (!initialized_) {
        return;
    }

    std::cout << "Shutting down Helix daemon..." << std::endl;

    // Save current module states for restoration on next start (best-effort)
    try {
        (void)save_module_states();
    } catch (const std::exception& e) {
        std::cerr << "Failed to save module states: " << e.what() << std::endl;
    }

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
        set_last_error("Daemon not initialized");
        return false;
    }

    // Supported input:
    // 1) A .helx archive (tar.gz) produced by helxcompiler
    std::cout << "Installing module from: " << package_path << std::endl;

    std::string source_dir = package_path;
    ModuleManifest manifest;

    // Only accept .helx
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

            // Extract archive using exec (avoid shell)
            int rc = run_program({"tar", "-xzf", package_path, "-C", temp_dir});
            if (rc != 0) {
                std::cerr << "Failed to extract .helx package: exit code " << rc << std::endl;
                set_last_error("Extract failed: tar exit code " + std::to_string(rc));
                std::filesystem::remove_all(temp_dir);
                return false;
            }
            source_dir = temp_dir;

            // Parse manifest from extracted temp
            if (!load_module_manifest(source_dir, manifest)) {
                std::cerr << "Failed to load manifest from extracted package" << std::endl;
                set_last_error("Manifest parse failed");
                std::filesystem::remove_all(temp_dir);
                return false;
            }

            // Enforce minimum core version requirement before installing
            if (!manifest.minimum_core_version.empty()) {
                const std::string core_version = std::string(HELIX_CORE_VERSION);
                const std::string requirement = std::string(">=") + manifest.minimum_core_version;
                if (!DependencyResolver::version_satisfies(core_version, requirement)) {
                    std::cerr << "Install refused: module '" << manifest.name
                              << "' requires Helix core >= " << manifest.minimum_core_version
                              << ", but running core is " << core_version << std::endl;
                    set_last_error("Core version " + std::string(HELIX_CORE_VERSION) +
                                   " does not satisfy >=" + manifest.minimum_core_version);
                    std::filesystem::remove_all(source_dir);
                    return false;
                }
            }

            // Enforce minimum API version requirement before installing
            if (!manifest.minimum_api_version.empty()) {
                const std::string api_version = std::string(HELIX_API_VERSION);
                const std::string requirement = std::string(">=") + manifest.minimum_api_version;
                if (!DependencyResolver::version_satisfies(api_version, requirement)) {
                    std::cerr << "Install refused: module '" << manifest.name
                              << "' requires Helix API >= " << manifest.minimum_api_version
                              << ", but running API is " << api_version << std::endl;
                    set_last_error("API version " + std::string(HELIX_API_VERSION) +
                                   " does not satisfy >=" + manifest.minimum_api_version);
                    std::filesystem::remove_all(source_dir);
                    return false;
                }
            }

            // Move extracted contents into final module install location (by name)
            std::string module_path = extract_package(source_dir, manifest.name);
            std::filesystem::remove_all(source_dir); // cleanup temp

            if (module_path.empty()) {
                std::cerr << "Failed to install extracted package" << std::endl;
                set_last_error("Install to modules dir failed");
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

    std::cerr << "Install failed: only .helx packages are supported" << std::endl;
    set_last_error("Unsupported package type (expected .helx)");
    return false;
}

bool HelixDaemon::uninstall_module(const std::string& module_name) {
    if (!initialized_) {
        std::cerr << "Daemon not initialized" << std::endl;
        return false;
    }

    auto it = module_registry_.find(module_name);
    if (it == module_registry_.end()) {
        std::cerr << "Module '" << module_name << "' is not installed" << std::endl;
        set_last_error("Not installed: " + module_name);
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
        set_last_error("Dependents present");
        return false;
    }

    // Disable module if it's enabled
    if (it->second.state != ModuleState::INSTALLED) {
        if (!disable_module(module_name)) {
            std::cerr << "Failed to disable module before uninstallation" << std::endl;
            set_last_error("Disable before uninstall failed");
            return false;
        }
    }

    // Remove module files
    if (!remove_module_files(module_name)) {
        std::cerr << "Failed to remove module files" << std::endl;
        set_last_error("Filesystem remove failed");
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
        set_last_error("Not installed: " + module_name);
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::INSTALLED) {
        std::cerr << "Module '" << module_name << "' is already enabled" << std::endl;
        set_last_error("Already enabled");
        return false;
    }

    // Resolve and load (and start) dependencies first
    if (!resolve_and_load_dependencies(module_name)) {
        // Do not transition to ERROR on dependency issues; leave as INSTALLED so the user can retry
        if (last_error_.empty()) set_last_error("Dependency resolution failed");
        std::cerr << "Enable aborted for '" << module_name << "': dependencies not satisfied" << std::endl;
        return false;
    }

    // Load the module
    std::string binary_path = module_info.install_path + "/" + module_info.manifest.binary_path;
    if (!module_loader_->load_module(binary_path, module_name, module_info.manifest.entry_points)) {
        // Leave module in INSTALLED state to allow retry/uninstall
        update_module_state(module_name, ModuleState::INSTALLED, "Failed to load module binary");
        set_last_error("Load failed: " + binary_path);
        return false;
    }

    update_module_state(module_name, ModuleState::LOADED);

    // Initialize the module
    if (!module_loader_->initialize_module(module_name)) {
        // Best-effort unload and reset state
        (void)module_loader_->unload_module(module_name);
        update_module_state(module_name, ModuleState::INSTALLED, "Failed to initialize module");
        set_last_error("Initialize failed");
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
        set_last_error("Not installed: " + module_name);
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

    // Unload the module if it was actually loaded; otherwise just reset state
    if (module_info.state == ModuleState::LOADED || module_info.state == ModuleState::INITIALIZED || module_info.state == ModuleState::RUNNING || module_info.state == ModuleState::STOPPED) {
        if (!module_loader_->unload_module(module_name)) {
            update_module_state(module_name, ModuleState::ERROR, "Failed to unload module");
            set_last_error("Unload failed");
            return false;
        }
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
        set_last_error("Not installed: " + module_name);
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::INITIALIZED && module_info.state != ModuleState::STOPPED) {
        std::cerr << "Module '" << module_name << "' must be enabled before starting" << std::endl;
        set_last_error("Not enabled");
        return false;
    }

    if (!module_loader_->start_module(module_name)) {
        // Do not leave in ERROR; remain INITIALIZED to allow retry or stop/disable
        update_module_state(module_name, ModuleState::INITIALIZED, "Failed to start module");
        set_last_error("Start failed");
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
        set_last_error("Not installed: " + module_name);
        return false;
    }

    auto& module_info = it->second;
    if (module_info.state != ModuleState::RUNNING) {
        std::cerr << "Module '" << module_name << "' is not running" << std::endl;
        set_last_error("Not running");
        return false;
    }

    if (!module_loader_->stop_module(module_name)) {
        update_module_state(module_name, ModuleState::ERROR, "Failed to stop module");
        set_last_error("Stop failed");
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
                // Only consider modules that were installed from .helx (marker file)
                if (!std::filesystem::exists(entry.path() / ".helx_installed")) {
                    continue;
                }
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
        // If destination exists (reinstall/upgrade), verify it belongs to the same module
        std::error_code dec;
        if (std::filesystem::exists(destination, dec)) {
            // Try to read existing manifest to confirm identity
            ModuleManifest existing;
            bool ok = false;
            try { ok = load_module_manifest(destination, existing); } catch (...) { ok = false; }
            if (ok && !existing.name.empty() && existing.name != module_name) {
                std::cerr << "Refusing to overwrite existing module directory '" << destination
                          << "' which belongs to '" << existing.name << "'" << std::endl;
                return std::string();
            }
            // Safe to remove: same module or unreadable; proceed
            std::filesystem::remove_all(destination, dec);
        }
        if (package_path != destination) {
            std::filesystem::copy(package_path, destination,
                                  std::filesystem::copy_options::recursive);
        }
        // Write install marker
        std::ofstream marker(destination + "/.helx_installed");
        marker << "installed_by=helxcompiler\n";
        marker.close();
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
        // Build a detailed error for clients
        std::ostringstream err;
        err << "Dependency resolution failed for '" << module_name << "'";
        // Declared dependencies from manifest
        auto it_mod = module_registry_.find(module_name);
        if (it_mod != module_registry_.end()) {
            const auto& deps = it_mod->second.manifest.dependencies;
            if (!deps.empty()) {
                err << "; required: ";
                for (size_t i = 0; i < deps.size(); ++i) {
                    err << deps[i].name;
                    if (i + 1 < deps.size()) err << ", ";
                }
            }
        }
        if (!result.missing_deps.empty()) {
            err << "; missing: ";
            for (size_t i = 0; i < result.missing_deps.size(); ++i) {
                err << result.missing_deps[i];
                if (i + 1 < result.missing_deps.size()) err << ", ";
            }
        }
        if (!result.circular_deps.empty()) {
            err << "; circular: ";
            for (size_t i = 0; i < result.circular_deps.size(); ++i) {
                err << result.circular_deps[i];
                if (i + 1 < result.circular_deps.size()) err << ", ";
            }
        }
        set_last_error(err.str());
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
                set_last_error(std::string("Failed to enable dependency '") + dep_name + "': " + last_error_);
                return false;
            }
        }
        // Ensure dependency is running (not just enabled), per requirement
        dep_it = module_registry_.find(dep_name);
        if (dep_it != module_registry_.end() && dep_it->second.state != ModuleState::RUNNING) {
            // If it's initialized or stopped, start it
            if (dep_it->second.state == ModuleState::INITIALIZED || dep_it->second.state == ModuleState::STOPPED) {
                if (!start_module(dep_name)) {
                    set_last_error(std::string("Failed to start dependency '") + dep_name + "': " + last_error_);
                    return false;
                }
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

ModuleState HelixDaemon::state_from_string(const std::string& state_str) {
    if (state_str == "Unknown") return ModuleState::UNKNOWN;
    if (state_str == "Installed") return ModuleState::INSTALLED;
    if (state_str == "Loaded") return ModuleState::LOADED;
    if (state_str == "Initialized") return ModuleState::INITIALIZED;
    if (state_str == "Running") return ModuleState::RUNNING;
    if (state_str == "Stopped") return ModuleState::STOPPED;
    if (state_str == "Error") return ModuleState::ERROR;
    return ModuleState::UNKNOWN;
}

std::string HelixDaemon::state_file_path() const {
    return modules_directory_ + "/.helix_state.json";
}

bool HelixDaemon::save_module_states() const {
    // We persist only high-level states we can restore: Installed, Initialized, Running, Stopped
    // Error/Loaded will be treated conservatively.
    const std::string path = state_file_path();
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Could not open state file for writing: " << path << std::endl;
        return false;
    }

    ofs << "{\n  \"modules\": {\n";
    size_t count = 0;
    const size_t total = module_registry_.size();
    for (const auto& kv : module_registry_) {
        const auto& name = kv.first;
        const auto& info = kv.second;
        ofs << "    \"" << name << "\": { \"state\": \"" << state_to_string(info.state) << "\" }";
        if (++count < total) ofs << ",";
        ofs << "\n";
    }
    ofs << "  }\n}";
    ofs.close();
    std::cout << "Saved module states to '" << path << "'" << std::endl;
    return true;
}

bool HelixDaemon::load_saved_module_states(std::unordered_map<std::string, ModuleState>& out_states) const {
    out_states.clear();
    const std::string path = state_file_path();
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        // No state saved yet is not an error
        return true;
    }
    std::stringstream buf; buf << ifs.rdbuf();
    const std::string content = buf.str();
    ifs.close();

    // Extremely small and tolerant parser.
    // We specifically scope to the top-level object at key "modules" and parse entries of the form:
    //   "name": { "state": "Value" }
    // This avoids bringing in a JSON dependency while ensuring we don't accidentally treat
    // the top-level key "modules" itself as a module entry.
    try {
        // Locate the "modules" object block
        const std::string modules_key = "\"modules\"";
        size_t key_pos = content.find(modules_key);
        if (key_pos == std::string::npos) {
            // Be tolerant: treat as having nothing to restore
            std::cerr << "State file '" << path << "' has no 'modules' key" << std::endl;
            return true;
        }

        // Find the opening brace for the modules object after the key
        size_t brace_start = content.find('{', key_pos + modules_key.size());
        if (brace_start == std::string::npos) {
            std::cerr << "State file '" << path << "': malformed 'modules' object" << std::endl;
            return true;
        }

        // Find the matching closing brace by tracking nesting
        size_t i = brace_start;
        int depth = 0;
        bool in_string = false;
        for (; i < content.size(); ++i) {
            char c = content[i];
            if (c == '"') {
                // Toggle in_string if not escaped
                bool escaped = (i > 0 && content[i-1] == '\\');
                if (!escaped) in_string = !in_string;
            }
            if (in_string) continue;
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    break; // 'i' is the matching closing brace
                }
            }
        }

        if (i >= content.size()) {
            std::cerr << "State file '" << path << "': unterminated 'modules' object" << std::endl;
            return true;
        }

        const size_t brace_end = i; // inclusive index of '}'
        const std::string modules_block = content.substr(brace_start + 1, brace_end - brace_start - 1);

        // Now parse immediate entries within the modules block
        // Raw string for clarity; ECMAScript regex used by std::regex requires \} to match a literal '}'
        std::regex entry_regex(R"regex("([^"]+)"\s*:\s*\{[^}]*"state"\s*:\s*"([^"]+)"[^}]*\})regex");
        std::sregex_iterator it(modules_block.begin(), modules_block.end(), entry_regex);
        std::sregex_iterator end;
        size_t parsed = 0;
        for (; it != end; ++it) {
            const std::string name = (*it)[1].str();
            const std::string state_s = (*it)[2].str();
            if (name == "modules") {
                // Defensive: skip any accidental capture of the container key
                continue;
            }
            out_states[name] = state_from_string(state_s);
            ++parsed;
        }
        if (parsed == 0) {
            std::cerr << "State file '" << path << "' contained no module entries" << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse state file: " << e.what() << std::endl;
        return false;
    }
}

void HelixDaemon::restore_saved_states(const std::unordered_map<std::string, ModuleState>& saved_states) {
    if (saved_states.empty()) return;

    // First, enable modules that were at least enabled previously (Initialized/Running/Stopped)
    // Compute a global dependency-aware order.
    std::vector<std::string> to_enable_vec;
    to_enable_vec.reserve(saved_states.size());
    for (const auto& [name, desired_state] : saved_states) {
        if (desired_state == ModuleState::INITIALIZED || desired_state == ModuleState::RUNNING || desired_state == ModuleState::STOPPED) {
            if (module_registry_.find(name) != module_registry_.end()) {
                to_enable_vec.push_back(name);
            } else {
                std::cout << "Skipping restore for '" << name << "': not installed" << std::endl;
            }
        }
    }

    if (!to_enable_vec.empty()) {
        auto res = dependency_resolver_->resolve_dependencies(to_enable_vec);
        if (!res.success) {
            std::cerr << "Restore: dependency resolution reported issues; proceeding with simple order" << std::endl;
        }
        const auto& order = res.load_order.empty() ? to_enable_vec : res.load_order;
        std::set<std::string> enable_set(to_enable_vec.begin(), to_enable_vec.end());
        for (const auto& name : order) {
            auto it = module_registry_.find(name);
            if (it == module_registry_.end()) continue;
            // Only enable if it was desired (or it is a dependency needed to reach desired modules)
            if (it->second.state == ModuleState::INSTALLED) {
                bool ok = enable_module(name);
                if (!ok) {
                    std::cerr << "Restore: enable failed for '" << name << "': " << last_error_ << std::endl;
                }
            }
        }
    }

    // Next, start modules that were running previously (dependency-aware order as well)
    std::vector<std::string> to_start_vec;
    to_start_vec.reserve(saved_states.size());
    for (const auto& [name, desired_state] : saved_states) {
        if (desired_state == ModuleState::RUNNING && module_registry_.find(name) != module_registry_.end()) {
            to_start_vec.push_back(name);
        }
    }
    if (!to_start_vec.empty()) {
        auto res2 = dependency_resolver_->resolve_dependencies(to_start_vec);
        const auto& order2 = res2.load_order.empty() ? to_start_vec : res2.load_order;
        for (const auto& name : order2) {
            auto it = module_registry_.find(name);
            if (it == module_registry_.end()) continue;
            if (it->second.state == ModuleState::INITIALIZED || it->second.state == ModuleState::STOPPED) {
                bool ok = start_module(name);
                if (!ok) {
                    std::cerr << "Restore: start failed for '" << name << "': " << last_error_ << std::endl;
                }
            }
        }
    }
}

} // namespace helix