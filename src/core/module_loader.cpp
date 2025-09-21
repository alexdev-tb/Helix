#include "helix/module_loader.h"
#include <dlfcn.h>
#include <iostream>
#include <vector>

namespace helix {

ModuleLoader::ModuleLoader() {
}

ModuleLoader::~ModuleLoader() {
    // Unload all modules on destruction
    for (auto& [name, module] : loaded_modules_) {
        if (module->running) {
            stop_module(name);
        }
        if (module->handle) {
            dlclose(module->handle);
        }
    }
    loaded_modules_.clear();
}

bool ModuleLoader::load_module(const std::string& module_path, const std::string& module_name) {
    EntryPoints eps;
    return load_module(module_path, module_name, eps);
}

bool ModuleLoader::load_module(const std::string& module_path, const std::string& module_name,
                               const EntryPoints& entry_points) {
    if (loaded_modules_.find(module_name) != loaded_modules_.end()) {
        std::cerr << "Module '" << module_name << "' is already loaded" << std::endl;
        return false;
    }

    // Load the shared library with RTLD_GLOBAL so symbols are visible to other modules
    void* handle = dlopen(module_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "Failed to load module '" << module_name << "': " << dlerror() << std::endl;
        return false;
    }

    auto module_info = std::make_unique<ModuleInfo>();
    module_info->name = module_name;
    module_info->path = module_path;
    module_info->handle = handle;
    module_info->initialized = false;
    module_info->running = false;

    if (!resolve_entry_points(handle, module_info->interface, entry_points)) {
        std::cerr << "Failed to resolve entry points for module '" << module_name << "'" << std::endl;
        dlclose(handle);
        return false;
    }

    loaded_modules_[module_name] = std::move(module_info);
    
    std::cout << "Successfully loaded module '" << module_name << "' from " << module_path << std::endl;
    return true;
}

bool ModuleLoader::unload_module(const std::string& module_name) {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        std::cerr << "Module '" << module_name << "' is not loaded" << std::endl;
        return false;
    }

    auto& module = it->second;

    if (module->running) {
        if (!stop_module(module_name)) {
            std::cerr << "Failed to stop module '" << module_name << "' before unloading" << std::endl;
            return false;
        }
    }

    if (module->initialized && module->interface.destroy) {
        module->interface.destroy();
    }

    if (dlclose(module->handle) != 0) {
        std::cerr << "Failed to unload module '" << module_name << "': " << dlerror() << std::endl;
        return false;
    }

    loaded_modules_.erase(it);
    
    std::cout << "Successfully unloaded module '" << module_name << "'" << std::endl;
    return true;
}

bool ModuleLoader::initialize_module(const std::string& module_name) {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        std::cerr << "Module '" << module_name << "' is not loaded" << std::endl;
        return false;
    }

    auto& module = it->second;
    
    if (module->initialized) {
        std::cerr << "Module '" << module_name << "' is already initialized" << std::endl;
        return false;
    }

    if (!module->interface.init) {
        std::cerr << "Module '" << module_name << "' does not have an init function" << std::endl;
        return false;
    }

    int result = module->interface.init();
    if (result != 0) {
        std::cerr << "Module '" << module_name << "' init function failed with code: " << result << std::endl;
        return false;
    }

    module->initialized = true;
    std::cout << "Successfully initialized module '" << module_name << "'" << std::endl;
    return true;
}

bool ModuleLoader::start_module(const std::string& module_name) {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        std::cerr << "Module '" << module_name << "' is not loaded" << std::endl;
        return false;
    }

    auto& module = it->second;
    
    if (!module->initialized) {
        std::cerr << "Module '" << module_name << "' must be initialized before starting" << std::endl;
        return false;
    }

    if (module->running) {
        std::cerr << "Module '" << module_name << "' is already running" << std::endl;
        return false;
    }

    if (!module->interface.start) {
        std::cerr << "Module '" << module_name << "' does not have a start function" << std::endl;
        return false;
    }

    int result = module->interface.start();
    if (result != 0) {
        std::cerr << "Module '" << module_name << "' start function failed with code: " << result << std::endl;
        return false;
    }

    module->running = true;
    std::cout << "Successfully started module '" << module_name << "'" << std::endl;
    return true;
}

bool ModuleLoader::stop_module(const std::string& module_name) {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        std::cerr << "Module '" << module_name << "' is not loaded" << std::endl;
        return false;
    }

    auto& module = it->second;
    
    if (!module->running) {
        std::cerr << "Module '" << module_name << "' is not running" << std::endl;
        return false;
    }

    if (!module->interface.stop) {
        std::cerr << "Module '" << module_name << "' does not have a stop function" << std::endl;
        return false;
    }

    // Call the module's stop function
    int result = module->interface.stop();
    if (result != 0) {
        std::cerr << "Module '" << module_name << "' stop function failed with code: " << result << std::endl;
        return false;
    }

    module->running = false;
    std::cout << "Successfully stopped module '" << module_name << "'" << std::endl;
    return true;
}

bool ModuleLoader::is_module_loaded(const std::string& module_name) const {
    return loaded_modules_.find(module_name) != loaded_modules_.end();
}

bool ModuleLoader::is_module_running(const std::string& module_name) const {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        return false;
    }
    return it->second->running;
}

const ModuleInfo* ModuleLoader::get_module_info(const std::string& module_name) const {
    auto it = loaded_modules_.find(module_name);
    if (it == loaded_modules_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<std::string> ModuleLoader::get_loaded_modules() const {
    std::vector<std::string> module_names;
    module_names.reserve(loaded_modules_.size());
    
    for (const auto& [name, module] : loaded_modules_) {
        module_names.push_back(name);
    }
    
    return module_names;
}

bool ModuleLoader::resolve_entry_points(void* handle, ModuleInterface& interface, const EntryPoints& entry_points) {
    if (!validate_module_handle(handle)) {
        return false;
    }

    dlerror();

    typedef int (*init_func_t)();
    init_func_t init_func = reinterpret_cast<init_func_t>(dlsym(handle, entry_points.init.c_str()));
    if (!init_func) {
        std::cerr << "Required entry point '" << entry_points.init << "' not found: " << dlerror() << std::endl;
        return false;
    }
    interface.init = init_func;

    typedef int (*start_func_t)();
    start_func_t start_func = reinterpret_cast<start_func_t>(dlsym(handle, entry_points.start.c_str()));
    if (!start_func) {
        std::cerr << "Required entry point '" << entry_points.start << "' not found: " << dlerror() << std::endl;
        return false;
    }
    interface.start = start_func;

    typedef int (*stop_func_t)();
    stop_func_t stop_func = reinterpret_cast<stop_func_t>(dlsym(handle, entry_points.stop.c_str()));
    if (!stop_func) {
        std::cerr << "Required entry point '" << entry_points.stop << "' not found: " << dlerror() << std::endl;
        return false;
    }
    interface.stop = stop_func;

    typedef void (*destroy_func_t)();
    destroy_func_t destroy_func = reinterpret_cast<destroy_func_t>(dlsym(handle, entry_points.destroy.c_str()));
    if (!destroy_func) {
        std::cerr << "Required entry point '" << entry_points.destroy << "' not found: " << dlerror() << std::endl;
        return false;
    }
    interface.destroy = destroy_func;

    return true;
}

bool ModuleLoader::validate_module_handle(void* handle) const {
    return handle != nullptr;
}

} // namespace helix