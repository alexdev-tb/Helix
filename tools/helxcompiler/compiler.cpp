#include "compiler.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <archive.h>
#include <archive_entry.h>

namespace helix {

HelixCompiler::HelixCompiler() : last_error_("") {}

bool HelixCompiler::compile_module(const CompileConfig& config) {
    last_error_.clear();

    if (config.verbose) {
        std::cout << "Compiling module from: " << config.source_directory << "\n"
                  << "Output file: " << config.output_file << std::endl;
    }

    auto source_files = find_source_files(config.source_directory);
    if (source_files.empty()) {
        set_error("No source files found in directory: " + config.source_directory);
        return false;
    }

    if (config.verbose) {
        std::cout << "Found " << source_files.size() << " source files" << std::endl;
    }

    std::string temp_dir = "/tmp/helix_build_" + std::to_string(getpid());
    std::filesystem::create_directories(temp_dir);

    // Ensure module name present
    std::string mod_name = config.module_name;
    if (mod_name.empty()) {
        std::string dummy_ver;
        if (!extract_module_metadata(source_files, mod_name, dummy_ver)) {
            set_error("Could not determine module name from sources");
            std::filesystem::remove_all(temp_dir);
            return false;
        }
    }

    std::string so_file = temp_dir + "/lib" + mod_name + ".so";
    if (!compile_shared_library(config, source_files, so_file)) {
        std::filesystem::remove_all(temp_dir);
        return false;
    }

    std::string manifest_file = temp_dir + "/manifest.json";
    if (!generate_manifest(config, manifest_file)) {
        std::filesystem::remove_all(temp_dir);
        return false;
    }

    if (!create_helx_package(so_file, manifest_file, config.output_file)) {
        std::filesystem::remove_all(temp_dir);
        return false;
    }

    std::filesystem::remove_all(temp_dir);

    if (config.verbose) {
        std::cout << "Successfully created " << config.output_file << std::endl;
    }
    return true;
}

bool HelixCompiler::detect_module_config(const std::string& source_dir, CompileConfig& config) {
    config.source_directory = source_dir;

    auto source_files = find_source_files(source_dir);
    if (source_files.empty()) {
        set_error("No source files found in directory");
        return false;
    }

    std::string module_name, module_version;
    if (!extract_module_metadata(source_files, module_name, module_version)) {
        set_error("Could not extract module metadata from source files");
        return false;
    }

    config.module_name = module_name;
    config.module_version = module_version;

    if (config.output_file.empty()) {
        config.output_file = module_name + ".helx";
    }
    return true;
}

std::vector<std::string> HelixCompiler::find_source_files(const std::string& directory) {
    std::vector<std::string> source_files;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
                    source_files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        set_error(std::string("Error scanning directory: ") + e.what());
    }
    return source_files;
}

bool HelixCompiler::extract_module_metadata(const std::vector<std::string>& source_files,
                                           std::string& module_name,
                                           std::string& module_version) {
    // Match HELIX_MODULE_DECLARE("name", "version", ...)
    const std::regex name_pattern(R"regex(HELIX_MODULE_DECLARE\s*\(\s*"([^"]+)")regex");
    const std::regex version_pattern(R"regex(HELIX_MODULE_DECLARE\s*\(\s*"[^"]+"\s*,\s*"([^"]+)")regex");

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Collapse newlines to spaces for easier matching
        std::string normalized = content;
        std::replace(normalized.begin(), normalized.end(), '\n', ' ');
        std::replace(normalized.begin(), normalized.end(), '\r', ' ');

        std::smatch m;
        if (module_name.empty() && std::regex_search(normalized, m, name_pattern) && m.size() > 1) {
            module_name = m[1].str();
        }
        if (module_version.empty() && std::regex_search(normalized, m, version_pattern) && m.size() > 1) {
            module_version = m[1].str();
        }
        if (!module_name.empty() && !module_version.empty()) {
            return true;
        }
    }
    return !module_name.empty() && !module_version.empty();
}

bool HelixCompiler::compile_shared_library(const CompileConfig& config,
                                         const std::vector<std::string>& source_files,
                                         const std::string& output_so) {
    std::stringstream cmd;
    cmd << "g++";
    cmd << " -std=" << (config.cxx_standard.empty() ? "c++17" : config.cxx_standard);
    if (!config.optimization_level.empty()) cmd << " " << config.optimization_level; else cmd << " -O2";
    if (config.debug_info) cmd << " -g";
    cmd << " -shared -fPIC";

    // Auto-detect helix include dir by walking up from the first source file
    std::string base = source_files.empty() ? std::filesystem::current_path().string()
                                            : std::filesystem::path(source_files.front()).parent_path().string();
    std::string helixInclude = detect_helix_include(base);
    if (!helixInclude.empty()) {
        cmd << " -I" << helixInclude;
    }
    for (const auto& inc : config.include_paths) cmd << " -I" << inc;

    for (const auto& src : source_files) cmd << " " << src;

    for (const auto& libp : config.library_paths) cmd << " -L" << libp;
    for (const auto& lib : config.libraries) cmd << " -l" << lib;

    cmd << " -pthread -ldl";
    cmd << " -o " << output_so;

    if (config.verbose) std::cout << "Running: " << cmd.str() << std::endl;

    std::string output;
    int rc = run_command(cmd.str(), output);
    if (rc != 0) {
        set_error(std::string("Compilation failed: ") + output);
        return false;
    }
    return true;
}

bool HelixCompiler::generate_manifest(const CompileConfig& config, const std::string& manifest_path) {
    std::ofstream manifest(manifest_path);
    if (!manifest.is_open()) {
        set_error("Failed to create manifest file");
        return false;
    }

    const std::string name = config.module_name.empty() ? "unknown" : config.module_name;
    const std::string version = config.module_version.empty() ? "1.0.0" : config.module_version;

    manifest << "{\n";
    manifest << "  \"name\": \"" << name << "\",\n";
    manifest << "  \"version\": \"" << version << "\",\n";
    manifest << "  \"description\": \"Module compiled with helxcompiler\",\n";
    manifest << "  \"author\": \"Unknown\",\n";
    manifest << "  \"license\": \"MIT\",\n";
    manifest << "  \"binary_path\": \"lib" << name << ".so\",\n";
    manifest << "  \"api_version\": \"1.0.0\",\n";
    manifest << "  \"entry_points\": {\n";
    manifest << "    \"init\": \"helix_module_init\",\n";
    manifest << "    \"start\": \"helix_module_start\",\n";
    manifest << "    \"stop\": \"helix_module_stop\",\n";
    manifest << "    \"destroy\": \"helix_module_destroy\"\n";
    manifest << "  },\n";
    manifest << "  \"dependencies\": [],\n";
    manifest << "  \"capabilities\": []\n";
    manifest << "}\n";

    manifest.close();
    return true;
}

bool HelixCompiler::create_helx_package(const std::string& so_file,
                                       const std::string& manifest_file,
                                       const std::string& output_helx) {
    std::stringstream cmd;
    cmd << "tar -czf " << output_helx
        << " -C " << std::filesystem::path(so_file).parent_path().string()
        << " " << std::filesystem::path(so_file).filename().string()
        << " " << std::filesystem::path(manifest_file).filename().string();

    std::string output;
    int rc = run_command(cmd.str(), output);
    if (rc != 0) {
        set_error(std::string("Failed to create .helx package: ") + output);
        return false;
    }
    return true;
}

int HelixCompiler::run_command(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) { output = "Failed to execute command"; return -1; }
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) { output += buffer; }
    int result = pclose(pipe);
    return result;
}

void HelixCompiler::set_error(const std::string& error) {
    last_error_ = error;
    std::cerr << "HelixCompiler error: " << error << std::endl;
}

std::string HelixCompiler::detect_helix_include(const std::string& from_dir) const {
    // Try environment variable first
    if (const char* env = std::getenv("HELIX_ROOT")) {
        std::filesystem::path p(env);
        auto inc = p / "include";
        if (std::filesystem::exists(inc / "helix" / "module.h")) {
            return inc.string();
        }
    }

    // Walk up directories from from_dir to root and look for include/helix/module.h
    std::filesystem::path cur = from_dir;
    for (int i = 0; i < 6; ++i) {
        std::filesystem::path inc = cur / "include";
        if (std::filesystem::exists(inc / "helix" / "module.h")) {
            return inc.string();
        }
        if (cur.has_parent_path()) cur = cur.parent_path(); else break;
    }

    // Try relative to CWD as a fallback
    std::filesystem::path cwd = std::filesystem::current_path();
    if (std::filesystem::exists(cwd / "../../include/helix/module.h")) {
        return (cwd / "../../include").string();
    }

    return std::string();
}

} // namespace helix