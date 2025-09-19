#ifndef HELIX_MODULE_H
#define HELIX_MODULE_H

/**
 * @file module.h
 * @brief Helix Module Development Kit
 * 
 * This header provides macros and utilities for developing Helix modules.
 * It simplifies the process of defining module metadata and entry points.
 */

#include <string>
#include <iostream>

namespace helix {

/**
 * @brief Module context passed to module entry points
 */
struct ModuleContext {
    std::string module_name;
    std::string module_version;
    std::string install_path;
    void* user_data;  // Module-specific data
};

} // namespace helix

/**
 * @brief Declare module metadata
 * 
 * Use this macro at the beginning of your module source file to declare
 * the module's basic information.
 * 
 * Example:
 * HELIX_MODULE_DECLARE("my-module", "1.0.0", "A sample module", "Author Name")
 */
#define HELIX_MODULE_DECLARE(name, version, description, author) \
    static const char* _helix_module_name = name; \
    static const char* _helix_module_version = version; \
    static const char* _helix_module_description = description; \
    static const char* _helix_module_author = author; \
    \
    extern "C" { \
        const char* helix_module_get_name() { return _helix_module_name; } \
        const char* helix_module_get_version() { return _helix_module_version; } \
        const char* helix_module_get_description() { return _helix_module_description; } \
        const char* helix_module_get_author() { return _helix_module_author; } \
    }

/**
 * @brief Declare module dependencies
 * 
 * Use this macro to declare what other modules this module depends on.
 * 
 * Example:
 * HELIX_MODULE_DEPENDS({"core-utils", "1.0.0", false}, {"logger", "2.1.0", true})
 */
#define HELIX_MODULE_DEPENDS(...) \
    extern "C" { \
        const char* helix_module_get_dependencies() { \
            return "[" #__VA_ARGS__ "]"; \
        } \
    }

/**
 * @brief Declare module capabilities
 * 
 * Use this macro to declare what capabilities/permissions this module needs.
 * 
 * Example:
 * HELIX_MODULE_CAPABILITIES("network", "filesystem")
 */
#define HELIX_MODULE_CAPABILITIES(...) \
    extern "C" { \
        const char* helix_module_get_capabilities() { \
            return "[" #__VA_ARGS__ "]"; \
        } \
    }

/**
 * @brief Define module initialization function
 * 
 * Use this macro to define the module's initialization logic.
 * This is called once when the module is loaded.
 * 
 * Example:
 * HELIX_MODULE_INIT() {
 *     std::cout << "Module initializing..." << std::endl;
 *     return 0; // Success
 * }
 */
#define HELIX_MODULE_INIT() \
    extern "C" int helix_module_init()

/**
 * @brief Define module start function
 * 
 * Use this macro to define what happens when the module starts.
 * This is called when the module should begin its main operation.
 * 
 * Example:
 * HELIX_MODULE_START() {
 *     std::cout << "Module starting..." << std::endl;
 *     return 0; // Success
 * }
 */
#define HELIX_MODULE_START() \
    extern "C" int helix_module_start()

/**
 * @brief Define module stop function
 * 
 * Use this macro to define what happens when the module stops.
 * This is called when the module should stop its main operation.
 * 
 * Example:
 * HELIX_MODULE_STOP() {
 *     std::cout << "Module stopping..." << std::endl;
 *     return 0; // Success
 * }
 */
#define HELIX_MODULE_STOP() \
    extern "C" int helix_module_stop()

/**
 * @brief Define module cleanup function
 * 
 * Use this macro to define the module's cleanup logic.
 * This is called once when the module is being unloaded.
 * 
 * Example:
 * HELIX_MODULE_DESTROY() {
 *     std::cout << "Module cleaning up..." << std::endl;
 * }
 */
#define HELIX_MODULE_DESTROY() \
    extern "C" void helix_module_destroy()

/**
 * @brief Helper macro to get module context
 * 
 * This can be used within module functions to access module metadata.
 */
#define HELIX_MODULE_CONTEXT() \
    helix::ModuleContext { \
        _helix_module_name, \
        _helix_module_version, \
        "", /* install_path will be set by daemon */ \
        nullptr /* user_data */ \
    }

/**
 * @brief Log a message from the module
 * 
 * Example:
 * HELIX_MODULE_LOG("Module is doing something important");
 */
#define HELIX_MODULE_LOG(message) \
    std::cout << "[" << _helix_module_name << "] " << message << std::endl

/**
 * @brief Log an error from the module
 * 
 * Example:
 * HELIX_MODULE_ERROR("Something went wrong!");
 */
#define HELIX_MODULE_ERROR(message) \
    std::cerr << "[" << _helix_module_name << "] ERROR: " << message << std::endl

#endif // HELIX_MODULE_H