#ifndef HELIX_DAEMON_H
#define HELIX_DAEMON_H

#include "helix/module_loader.h"
#include "helix/dependency_resolver.h"
#include "helix/manifest.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace helix {

/**
 * @brief Module state in the daemon
 */
enum class ModuleState {
    UNKNOWN,     ///< Module state unknown
    INSTALLED,   ///< Module installed but not loaded
    LOADED,      ///< Module loaded but not initialized
    INITIALIZED, ///< Module initialized but not started
    RUNNING,     ///< Module running
    STOPPED,     ///< Module stopped but still initialized
    ERROR        ///< Module in error state
};

/**
 * @brief Information about a module known to the daemon
 */
struct DaemonModuleInfo {
    std::string name;
    std::string version;
    std::string install_path;  ///< Path where module is installed
    ModuleManifest manifest;
    ModuleState state;
    std::string error_message; ///< Last error message if any
};

/**
 * @brief Main Helix daemon
 * 
 * Manages the complete lifecycle of modules: installation, loading,
 * dependency resolution, starting, stopping, and uninstallation.
 */
class HelixDaemon {
public:
    HelixDaemon();
    ~HelixDaemon();

    /**
     * @brief Initialize the daemon
     * @param modules_directory Directory where modules are installed
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize(const std::string& modules_directory);

    /**
     * @brief Shutdown the daemon gracefully
     */
    void shutdown();

    /**
     * @brief Install a module from a .helx package
     * @param package_path Path to the .helx package file
     * @return true if installation succeeded, false otherwise
     */
    bool install_module(const std::string& package_path);

    /**
     * @brief Uninstall a module
     * @param module_name Name of the module to uninstall
     * @return true if uninstallation succeeded, false otherwise
     */
    bool uninstall_module(const std::string& module_name);

    /**
     * @brief Enable a module (load and initialize it)
     * @param module_name Name of the module to enable
     * @return true if enabling succeeded, false otherwise
     */
    bool enable_module(const std::string& module_name);

    /**
     * @brief Disable a module (stop and unload it)
     * @param module_name Name of the module to disable
     * @return true if disabling succeeded, false otherwise
     */
    bool disable_module(const std::string& module_name);

    /**
     * @brief Start a module (must be enabled first)
     * @param module_name Name of the module to start
     * @return true if starting succeeded, false otherwise
     */
    bool start_module(const std::string& module_name);

    /**
     * @brief Stop a module
     * @param module_name Name of the module to stop
     * @return true if stopping succeeded, false otherwise
     */
    bool stop_module(const std::string& module_name);

    /**
     * @brief Get information about a module
     * @param module_name Name of the module
     * @return Pointer to module info if found, nullptr otherwise
     */
    const DaemonModuleInfo* get_module_info(const std::string& module_name) const;

    /**
     * @brief Get list of all known modules
     * @return Vector of module names
     */
    std::vector<std::string> list_modules() const;

    /**
     * @brief Get modules by state
     * @param state Module state to filter by
     * @return Vector of module names in the specified state
     */
    std::vector<std::string> list_modules_by_state(ModuleState state) const;

    /**
     * @brief Refresh module information from filesystem
     * @return true if refresh succeeded, false otherwise
     */
    bool refresh_modules();

    /**
     * @brief Get daemon status information
     * @return Status string
     */
    std::string get_status() const;

    /**
     * @brief Convert module state to string
     * @param state Module state
     * @return String representation of state
     */
    static std::string state_to_string(ModuleState state);

    /**
     * @brief Get last error message from the most recent failed operation
     */
    const std::string& last_error() const { return last_error_; }

private:
    std::string modules_directory_;
    std::unique_ptr<ModuleLoader> module_loader_;
    std::unique_ptr<DependencyResolver> dependency_resolver_;
    std::unordered_map<std::string, DaemonModuleInfo> module_registry_;
    bool initialized_;
    std::string last_error_;

    void set_last_error(const std::string& err) { last_error_ = err; }

    /**
     * @brief Scan modules directory and populate registry
     * @return true if scanning succeeded, false otherwise
     */
    bool scan_modules_directory();

    /**
     * @brief Load manifest from a module directory
     * @param module_path Path to module directory
     * @param manifest Output manifest
     * @return true if loading succeeded, false otherwise
     */
    bool load_module_manifest(const std::string& module_path, ModuleManifest& manifest);

    /**
     * @brief Extract a .helx package to the modules directory
     * @param package_path Path to the .helx package
     * @param module_name Name of the module being installed
     * @return Path to extracted module directory, empty string on failure
     */
    std::string extract_package(const std::string& package_path, const std::string& module_name);

    /**
     * @brief Remove module files from filesystem
     * @param module_name Name of the module to remove
     * @return true if removal succeeded, false otherwise
     */
    bool remove_module_files(const std::string& module_name);

    /**
     * @brief Update module state in registry
     * @param module_name Name of the module
     * @param new_state New state to set
     * @param error_message Optional error message
     */
    void update_module_state(const std::string& module_name, ModuleState new_state, 
                           const std::string& error_message = "");

    /**
     * @brief Resolve and load module dependencies
     * @param module_name Name of the module
     * @return true if dependencies were resolved successfully, false otherwise
     */
    bool resolve_and_load_dependencies(const std::string& module_name);
};

} // namespace helix

#endif // HELIX_DAEMON_H