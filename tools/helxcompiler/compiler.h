#ifndef HELIX_COMPILER_H
#define HELIX_COMPILER_H

#include <string>
#include <vector>
#include <memory>

namespace helix {

/**
 * @brief Module compilation configuration
 */
struct CompileConfig {
    std::string source_directory;    ///< Directory containing module source
    std::string output_file;         ///< Output .helx file path
    std::string module_name;         ///< Module name (auto-detected if empty)
    std::string module_version;      ///< Module version (auto-detected if empty)
    std::vector<std::string> include_paths;  ///< Additional include paths
    std::vector<std::string> library_paths; ///< Additional library paths
    std::vector<std::string> libraries;     ///< Libraries to link against
    std::string cxx_standard;       ///< C++ standard (default: c++17)
    std::string optimization_level;  ///< Optimization level (default: -O2)
    bool debug_info;                ///< Include debug information
    bool verbose;                   ///< Verbose output
    // Optional custom entry point symbols
    std::string ep_init;            ///< Symbol for init (default helix_module_init)
    std::string ep_start;           ///< Symbol for start
    std::string ep_stop;            ///< Symbol for stop
    std::string ep_destroy;         ///< Symbol for destroy
};

/**
 * @brief Module compiler for creating .helx packages
 */
class HelixCompiler {
public:
    HelixCompiler();
    ~HelixCompiler() = default;

    /**
     * @brief Compile a module from source directory to .helx package
     * @param config Compilation configuration
     * @return true if compilation succeeded, false otherwise
     */
    bool compile_module(const CompileConfig& config);

    /**
     * @brief Auto-detect module configuration from source directory
     * @param source_dir Source directory path
     * @param config Output configuration
     * @return true if detection succeeded, false otherwise
     */
    bool detect_module_config(const std::string& source_dir, CompileConfig& config);

    /**
     * @brief Validate manifest.json in a source directory (no build)
     */
    bool validate_manifest_in_dir(const CompileConfig& config);

    /**
     * @brief Get the last error message
     * @return Last error message
     */
    const std::string& get_last_error() const { return last_error_; }
private:
    std::string last_error_;

    /**
     * @brief Find all source files in directory
     * @param directory Directory to search
     * @return Vector of source file paths
     */
    std::vector<std::string> find_source_files(const std::string& directory);

    /**
     * @brief Extract module metadata from source files
     * @param source_files List of source files
     * @param module_name Output module name
     * @param module_version Output module version
     * @return true if metadata found, false otherwise
     */
    bool extract_module_metadata(const std::vector<std::string>& source_files,
                                std::string& module_name,
                                std::string& module_version);

    /**
     * @brief Compile source files to shared library
     * @param config Compilation configuration
     * @param source_files List of source files
     * @param output_so Output shared library path
     * @return true if compilation succeeded, false otherwise
     */
    bool compile_shared_library(const CompileConfig& config,
                               const std::vector<std::string>& source_files,
                               const std::string& output_so);

    /**
     * @brief Generate manifest.json from module metadata
     * @param config Compilation configuration
     * @param manifest_path Output manifest file path
     * @return true if generation succeeded, false otherwise
     */
    bool generate_manifest(const CompileConfig& config, const std::string& manifest_path);

    /**
     * @brief Create .helx package from compiled files
     * @param so_file Compiled shared library path
     * @param manifest_file Manifest file path
     * @param output_helx Output .helx package path
     * @return true if packaging succeeded, false otherwise
     */
    bool create_helx_package(const std::string& so_file,
                           const std::string& manifest_file,
                           const std::string& output_helx);

    /**
     * @brief Run a program (no shell) and capture stdout+stderr
     * @param args argv list (args[0] must be program name, e.g., "g++")
     * @param output Combined stdout+stderr
     * @return Exit code (0 on success). -1 on exec/fork error.
     */
    int run_program_capture(const std::vector<std::string>& args, std::string& output);

    /**
     * @brief Set error message
     * @param error Error message
     */
    void set_error(const std::string& error);

    /**
     * @brief Detect the Helix include path
     * @param from_dir Directory to detect from
     * @return Detected Helix include path
     */
    std::string detect_helix_include(const std::string& from_dir) const;
};

} // namespace helix

#endif // HELIX_COMPILER_H