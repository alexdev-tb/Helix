#include "compiler.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#ifdef HELIX_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <archive.h>
#include <archive_entry.h>
#include "helix/manifest.h"
#include "helix/version.h"

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
        std::filesystem::path manifest_path = std::filesystem::path(source_dir) / "manifest.json";
        if (std::filesystem::exists(manifest_path)) {
            std::ifstream mf(manifest_path);
            std::string json_text((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
#ifdef HELIX_USE_NLOHMANN_JSON
            try {
                json j = json::parse(json_text);
                if (module_name.empty() && j.contains("name")) module_name = j.at("name").get<std::string>();
                if (module_version.empty() && j.contains("version")) module_version = j.at("version").get<std::string>();
            } catch (...) {
            }
#endif
            if (module_name.empty() || module_version.empty()) {
                std::smatch m;
                std::regex name_re("\\\"name\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
                std::regex ver_re("\\\"version\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
                if (module_name.empty() && std::regex_search(json_text, m, name_re) && m.size() > 1) module_name = m[1].str();
                if (module_version.empty() && std::regex_search(json_text, m, ver_re) && m.size() > 1) module_version = m[1].str();
            }
        }
        if (module_name.empty()) {
            set_error("Could not extract module name from sources or manifest");
            return false;
        }
        if (module_version.empty()) {
            set_error("Could not extract module version from sources or manifest");
            return false;
        }
    }

    config.module_name = module_name;
    config.module_version = module_version;

    {
        std::filesystem::path manifest_path = std::filesystem::path(source_dir) / "manifest.json";
        if (std::filesystem::exists(manifest_path)) {
            std::ifstream mf(manifest_path);
            std::string json_text((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
#ifdef HELIX_USE_NLOHMANN_JSON
            try {
                json j = json::parse(json_text);
                if (j.contains("entry_points") && j["entry_points"].is_object()) {
                    const auto& ep = j["entry_points"];
                    if (ep.contains("init") && config.ep_init.empty()) config.ep_init = ep.at("init").get<std::string>();
                    if (ep.contains("start") && config.ep_start.empty()) config.ep_start = ep.at("start").get<std::string>();
                    if (ep.contains("stop") && config.ep_stop.empty()) config.ep_stop = ep.at("stop").get<std::string>();
                    if (ep.contains("destroy") && config.ep_destroy.empty()) config.ep_destroy = ep.at("destroy").get<std::string>();
                }
            } catch (...) {
            }
#endif
            if (config.ep_init.empty() || config.ep_start.empty() || config.ep_stop.empty() || config.ep_destroy.empty()) {
                std::smatch obj;
                std::regex ep_obj(R"regex(\"entry_points\"\s*:\s*\{([\s\S]*?)\})regex");
                if (std::regex_search(json_text, obj, ep_obj) && obj.size() > 1) {
                    const std::string ep = obj[1].str();
                    std::smatch m;
                    auto getv = [&](const char* key) -> std::string {
                        std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
                        if (std::regex_search(ep, m, r) && m.size() > 1) return m[1].str();
                        return std::string();
                    };
                    std::string v;
                    if (config.ep_init.empty() && (v = getv("init")).size()) config.ep_init = v;
                    if (config.ep_start.empty() && (v = getv("start")).size()) config.ep_start = v;
                    if (config.ep_stop.empty() && (v = getv("stop")).size()) config.ep_stop = v;
                    if (config.ep_destroy.empty() && (v = getv("destroy")).size()) config.ep_destroy = v;
                }
            }
        }
    }

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
    const std::regex name_pattern(R"regex(HELIX_MODULE_DECLARE\s*\(\s*"([^"]+)")regex");
    const std::regex version_pattern(R"regex(HELIX_MODULE_DECLARE\s*\(\s*"[^"]+"\s*,\s*"([^"]+)")regex");

    for (const auto& file : source_files) {
        std::ifstream ifs(file);
        if (!ifs.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

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

    std::string base = source_files.empty() ? std::filesystem::current_path().string()
                                            : std::filesystem::path(source_files.front()).parent_path().string();
    std::string helixInclude = detect_helix_include(base);
    if (!helixInclude.empty()) {
        cmd << " -I" << helixInclude;
    }
    for (const auto& inc : config.include_paths) cmd << " -I" << inc;

    if (!config.module_name.empty()) {
        cmd << " -DHELIX_MODULE_NAME=\\\"" << config.module_name << "\\\"";
    }
    if (!config.module_version.empty()) {
        cmd << " -DHELIX_MODULE_VERSION=\\\"" << config.module_version << "\\\"";
    }

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

    std::string src_json;
    std::filesystem::path src_manifest = std::filesystem::path(config.source_directory) / "manifest.json";
    if (std::filesystem::exists(src_manifest)) {
        std::ifstream mf(src_manifest);
        src_json.assign((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
    }

    auto get_field = [&](const char* key) -> std::string {
        if (src_json.empty()) return std::string();
#ifdef HELIX_USE_NLOHMANN_JSON
        try {
            json j = json::parse(src_json);
            if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
        } catch (...) { }
#endif
        std::smatch m; std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        if (std::regex_search(src_json, m, r) && m.size() > 1) return m[1].str();
        return std::string();
    };
    auto get_array = [&](const char* key) -> std::string {
        if (src_json.empty()) return std::string();
#ifdef HELIX_USE_NLOHMANN_JSON
        try {
            json j = json::parse(src_json);
            if (j.contains(key) && j[key].is_array()) return j[key].dump().substr(1, j[key].dump().size()-2);
        } catch (...) { }
#endif
        std::smatch m; std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        if (std::regex_search(src_json, m, r) && m.size() > 1) return m[1].str();
        return std::string();
    };
    auto get_object = [&](const char* key) -> std::string {
        if (src_json.empty()) return std::string();
#ifdef HELIX_USE_NLOHMANN_JSON
        try {
            json j = json::parse(src_json);
            if (j.contains(key) && j[key].is_object()) {
                std::string s = j[key].dump();
                if (s.size() >= 2) return s.substr(1, s.size()-2);
                return std::string();
            }
        } catch (...) { }
#endif
        std::smatch m; std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
        if (std::regex_search(src_json, m, r) && m.size() > 1) return m[1].str();
        return std::string();
    };

    const std::string name = !config.module_name.empty() ? config.module_name : (get_field("name").empty() ? std::string("unknown") : get_field("name"));
    const std::string version = !config.module_version.empty() ? config.module_version : (get_field("version").empty() ? std::string("1.0.0") : get_field("version"));

    std::string description = get_field("description");
    std::string author = get_field("author");
    std::string license = get_field("license");
    std::string minimum_core_version = get_field("minimum_core_version");
    std::string minimum_api_version = get_field("minimum_api_version");
    std::string homepage = get_field("homepage");
    std::string repository = get_field("repository");
    std::string tags = get_array("tags");
    std::string config_obj = get_object("config");

    if (minimum_core_version.empty()) {
        minimum_core_version = HELIX_VERSION;
    }
    if (minimum_api_version.empty()) {
        minimum_api_version = HELIX_API_VERSION;
    }

    std::string ep_init = !config.ep_init.empty() ? config.ep_init : std::string();
    std::string ep_start = !config.ep_start.empty() ? config.ep_start : std::string();
    std::string ep_stop = !config.ep_stop.empty() ? config.ep_stop : std::string();
    std::string ep_destroy = !config.ep_destroy.empty() ? config.ep_destroy : std::string();
    if (ep_init.empty() || ep_start.empty() || ep_stop.empty() || ep_destroy.empty()) {
        std::string ep_obj = get_object("entry_points");
        if (!ep_obj.empty()) {
            auto get_ep = [&](const char* k){ std::smatch m; std::regex r(std::string("\\\"") + k + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
                if (std::regex_search(ep_obj, m, r) && m.size() > 1) return m[1].str(); return std::string(); };
            if (ep_init.empty()) ep_init = get_ep("init");
            if (ep_start.empty()) ep_start = get_ep("start");
            if (ep_stop.empty()) ep_stop = get_ep("stop");
            if (ep_destroy.empty()) ep_destroy = get_ep("destroy");
        }
    }
    if (ep_init.empty()) ep_init = "helix_module_init";
    if (ep_start.empty()) ep_start = "helix_module_start";
    if (ep_stop.empty()) ep_stop = "helix_module_stop";
    if (ep_destroy.empty()) ep_destroy = "helix_module_destroy";

    manifest << "{\n";
    manifest << "  \"name\": \"" << name << "\",\n";
    manifest << "  \"version\": \"" << version << "\",\n";
    if (!description.empty()) manifest << "  \"description\": \"" << description << "\",\n";
    if (!author.empty()) manifest << "  \"author\": \"" << author << "\",\n";
    if (!license.empty()) manifest << "  \"license\": \"" << license << "\",\n";
    manifest << "  \"binary_path\": \"lib" << name << ".so\",\n";
    manifest << "  \"minimum_core_version\": \"" << minimum_core_version << "\",\n";
    manifest << "  \"minimum_api_version\": \"" << minimum_api_version << "\",\n";
    manifest << "  \"entry_points\": {\n";
    manifest << "    \"init\": \"" << ep_init << "\",\n";
    manifest << "    \"start\": \"" << ep_start << "\",\n";
    manifest << "    \"stop\": \"" << ep_stop << "\",\n";
    manifest << "    \"destroy\": \"" << ep_destroy << "\"\n";
    manifest << "  },\n";
    std::string deps = get_array("dependencies");
    manifest << "  \"dependencies\": [";
    if (!deps.empty()) manifest << deps;
    manifest << "],\n";
    if (!tags.empty()) {
        manifest << "  \"tags\": [" << tags << "],\n";
    }
    if (!config_obj.empty()) {
        manifest << "  \"config\": {" << config_obj << "},\n";
    }
    if (!homepage.empty()) manifest << "  \"homepage\": \"" << homepage << "\",\n";
    if (!repository.empty()) manifest << "  \"repository\": \"" << repository << "\"\n";
    if (homepage.empty() && repository.empty()) {
    }
    manifest << "}\n";

    manifest.close();
    return true;
}

bool HelixCompiler::validate_manifest_in_dir(const CompileConfig& config) {
    std::filesystem::path manifest_path = std::filesystem::path(config.source_directory) / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        set_error("manifest.json not found in " + config.source_directory);
        return false;
    }
    helix::ManifestParser parser;
    helix::ModuleManifest mf;
    if (!parser.parse_from_file(manifest_path.string(), mf)) {
        set_error(parser.get_last_error());
        return false;
    }
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
    if (const char* env = std::getenv("HELIX_ROOT")) {
        std::filesystem::path p(env);
        auto inc = p / "include";
        if (std::filesystem::exists(inc / "helix" / "module.h")) {
            return inc.string();
        }
    }

    std::filesystem::path cur = from_dir;
    for (int i = 0; i < 6; ++i) {
        std::filesystem::path inc = cur / "include";
        if (std::filesystem::exists(inc / "helix" / "module.h")) {
            return inc.string();
        }
        if (cur.has_parent_path()) cur = cur.parent_path(); else break;
    }

    std::filesystem::path cwd = std::filesystem::current_path();
    if (std::filesystem::exists(cwd / "../../include/helix/module.h")) {
        return (cwd / "../../include").string();
    }

    return std::string();
}

} // namespace helix
