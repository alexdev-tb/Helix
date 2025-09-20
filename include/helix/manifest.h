#ifndef HELIX_MANIFEST_H
#define HELIX_MANIFEST_H

#include <string>
#include <vector>
#include <unordered_map>

namespace helix {

/**
 * @brief Customizable module entry point symbol names
 */
struct EntryPoints {
    std::string init = "helix_module_init";     ///< Init entry point symbol
    std::string start = "helix_module_start";   ///< Start entry point symbol
    std::string stop = "helix_module_stop";     ///< Stop entry point symbol
    std::string destroy = "helix_module_destroy"; ///< Destroy entry point symbol
};

/**
 * @brief Represents a module dependency
 */
struct Dependency {
    std::string name;        ///< Name of the required module
    std::string version;     ///< Required version (can include ranges like ">=1.0.0")
    bool optional;           ///< Whether this dependency is optional
};

/**
 * @brief Represents module metadata from a .helx manifest
 */
struct ModuleManifest {
    // Basic module information
    std::string name;        ///< Unique module name
    std::string version;     ///< Module version
    std::string description; ///< Human-readable description
    std::string author;      ///< Module author
    std::string license;     ///< License information

    // Technical details
    std::string binary_path;              ///< Path to the .so file within the package
    std::vector<Dependency> dependencies; ///< Required dependencies
    
    // Runtime configuration
    std::unordered_map<std::string, std::string> config; ///< Module-specific config
    std::vector<std::string> capabilities;                ///< Requested capabilities/permissions
    
    // Metadata
    std::string homepage;    ///< Module homepage URL
    std::string repository;  ///< Source repository URL
    std::vector<std::string> tags; ///< Searchable tags

    // Compatibility
    std::string minimum_core_version; ///< Minimum Helix core version required
    std::string minimum_api_version;  ///< Minimum Helix API version required

    // Entry points
    EntryPoints entry_points; ///< Optional custom entry points; defaults provided
};

/**
 * @brief Parser for Helix module manifest files
 * 
 * Handles parsing .helx module metadata in JSON format and validation
 * of module dependencies and configuration.
 */
class ManifestParser {
public:
    ManifestParser();
    ~ManifestParser() = default;

    /**
     * @brief Parse a manifest from a file
     * @param file_path Path to the manifest JSON file
     * @param manifest Output manifest structure
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_from_file(const std::string& file_path, ModuleManifest& manifest);

    /**
     * @brief Parse a manifest from a JSON string
     * @param json_content JSON string content
     * @param manifest Output manifest structure
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_from_string(const std::string& json_content, ModuleManifest& manifest);

    /**
     * @brief Validate a parsed manifest
     * @param manifest Manifest to validate
     * @return true if manifest is valid, false otherwise
     */
    bool validate_manifest(const ModuleManifest& manifest);

    /**
     * @brief Check if a version string is valid
     * @param version Version string to check
     * @return true if valid semantic version, false otherwise
     */
    static bool is_valid_version(const std::string& version);

    /**
     * @brief Check if a module name is valid
     * @param name Module name to check
     * @return true if valid name format, false otherwise
     */
    static bool is_valid_module_name(const std::string& name);

    /**
     * @brief Check if a C symbol name is valid
     * @param symbol Symbol name
     * @return true if valid C identifier for dlsym, false otherwise
     */
    static bool is_valid_symbol_name(const std::string& symbol);

    /**
     * @brief Serialize a manifest back to JSON string
     * @param manifest Manifest to serialize
     * @return JSON string representation
     */
    std::string serialize_manifest(const ModuleManifest& manifest);

    /**
     * @brief Get the last error message from parsing operations
     * @return Last error message
     */
    const std::string& get_last_error() const { return last_error_; }

private:
    std::string last_error_;

    /**
     * @brief Parse dependencies from JSON
     * @param deps_json JSON array of dependencies
     * @param dependencies Output vector of dependencies
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_dependencies(const std::string& deps_json, std::vector<Dependency>& dependencies);

    /**
     * @brief Parse configuration from JSON
     * @param config_json JSON object of configuration
     * @param config Output map of configuration
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_config(const std::string& config_json, std::unordered_map<std::string, std::string>& config);

    /**
     * @brief Parse string array from JSON
     * @param array_json JSON array of strings
     * @param output Output vector of strings
     * @return true if parsing succeeded, false otherwise
     */
    bool parse_string_array(const std::string& array_json, std::vector<std::string>& output);

    /**
     * @brief Set error message
     * @param error Error message to set
     */
    void set_error(const std::string& error);
};

} // namespace helix

#endif // HELIX_MANIFEST_H