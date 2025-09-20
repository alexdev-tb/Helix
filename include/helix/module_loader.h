#ifndef HELIX_MODULE_LOADER_H
#define HELIX_MODULE_LOADER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "helix/manifest.h"

namespace helix {

/**
 * @brief Standard module entry points that all Helix modules must implement
 */
struct ModuleInterface {
    std::function<int()> init;      ///< Initialize module resources
    std::function<int()> start;     ///< Start module operation
    std::function<int()> stop;      ///< Stop module operation
    std::function<void()> destroy;  ///< Cleanup module resources
};

/**
 * @brief Information about a loaded module
 */
struct ModuleInfo {
    std::string name;
    std::string version;
    std::string path;
    void* handle;               ///< dlopen handle
    ModuleInterface interface;  ///< Function pointers to module entry points
    bool initialized;
    bool running;
};

/**
 * @brief Core module loader for the Helix framework
 * 
 * Handles dynamic loading of compiled modules (.so files) using dlopen(),
 * resolves standard entry points, and manages module lifecycle.
 */
class ModuleLoader {
public:
    ModuleLoader();
    ~ModuleLoader();

    /**
     * @brief Load a module from a shared library file
     * @param module_path Path to the .so file
     * @param module_name Unique name for the module
     * @return true if loaded successfully, false otherwise
     */
    bool load_module(const std::string& module_path, const std::string& module_name);

    /**
     * @brief Load a module with custom entry point symbol names
     * @param module_path Path to the .so file
     * @param module_name Unique name for the module
     * @param entry_points Custom entry point symbols (init/start/stop/destroy)
     * @return true on success
     */
    bool load_module(const std::string& module_path, const std::string& module_name,
                     const EntryPoints& entry_points);

    /**
     * @brief Unload a previously loaded module
     * @param module_name Name of the module to unload
     * @return true if unloaded successfully, false otherwise
     */
    bool unload_module(const std::string& module_name);

    /**
     * @brief Initialize a loaded module (calls init entry point)
     * @param module_name Name of the module to initialize
     * @return true if initialized successfully, false otherwise
     */
    bool initialize_module(const std::string& module_name);

    /**
     * @brief Start a module (calls start entry point)
     * @param module_name Name of the module to start
     * @return true if started successfully, false otherwise
     */
    bool start_module(const std::string& module_name);

    /**
     * @brief Stop a module (calls stop entry point)
     * @param module_name Name of the module to stop
     * @return true if stopped successfully, false otherwise
     */
    bool stop_module(const std::string& module_name);

    /**
     * @brief Check if a module is loaded
     * @param module_name Name of the module to check
     * @return true if module is loaded, false otherwise
     */
    bool is_module_loaded(const std::string& module_name) const;

    /**
     * @brief Check if a module is running
     * @param module_name Name of the module to check
     * @return true if module is running, false otherwise
     */
    bool is_module_running(const std::string& module_name) const;

    /**
     * @brief Get information about a loaded module
     * @param module_name Name of the module
     * @return Pointer to ModuleInfo if found, nullptr otherwise
     */
    const ModuleInfo* get_module_info(const std::string& module_name) const;

    /**
     * @brief Get names of all loaded modules
     * @return Vector of module names
     */
    std::vector<std::string> get_loaded_modules() const;

private:
    std::unordered_map<std::string, std::unique_ptr<ModuleInfo>> loaded_modules_;

    /**
     * @brief Resolve standard entry points from a loaded module
     * @param handle dlopen handle
     * @param interface ModuleInterface to populate
     * @return true if all entry points found, false otherwise
     */
    bool resolve_entry_points(void* handle, ModuleInterface& interface, const EntryPoints& entry_points);

    /**
     * @brief Validate that a module handle is valid
     * @param handle dlopen handle to validate
     * @return true if valid, false otherwise
     */
    bool validate_module_handle(void* handle) const;
};

} // namespace helix

#endif // HELIX_MODULE_LOADER_H