#include "helix/manifest.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#ifdef HELIX_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using nlohmann::json;
#endif

namespace helix {

ManifestParser::ManifestParser() : last_error_("") {
}

bool ManifestParser::parse_from_file(const std::string& file_path, ModuleManifest& manifest) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        set_error("Failed to open manifest file: " + file_path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return parse_from_string(buffer.str(), manifest);
}

bool ManifestParser::parse_from_string(const std::string& json_content, ModuleManifest& manifest) {
#ifdef HELIX_USE_NLOHMANN_JSON
    last_error_.clear();
    try {
        json j = json::parse(json_content);
        // Required fields
        if (!j.contains("name") || !j.contains("version") || !j.contains("binary_path")) {
            set_error("Missing required field(s): name/version/binary_path");
            return false;
        }
        manifest.name = j.at("name").get<std::string>();
        manifest.version = j.at("version").get<std::string>();
        manifest.binary_path = j.at("binary_path").get<std::string>();

        // Optional strings
        if (j.contains("description")) manifest.description = j.at("description").get<std::string>();
        if (j.contains("author")) manifest.author = j.at("author").get<std::string>();
        if (j.contains("license")) manifest.license = j.at("license").get<std::string>();
        if (j.contains("homepage")) manifest.homepage = j.at("homepage").get<std::string>();
        if (j.contains("repository")) manifest.repository = j.at("repository").get<std::string>();
        if (j.contains("minimum_core_version")) manifest.minimum_core_version = j.at("minimum_core_version").get<std::string>();
        if (j.contains("minimum_api_version")) manifest.minimum_api_version = j.at("minimum_api_version").get<std::string>();

        // Arrays
        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& d : j["dependencies"]) {
                Dependency dep{};
                if (d.contains("name")) dep.name = d.at("name").get<std::string>();
                if (d.contains("version")) dep.version = d.at("version").get<std::string>();
                dep.optional = d.value("optional", false);
                manifest.dependencies.push_back(dep);
            }
        }
        if (j.contains("tags") && j["tags"].is_array()) {
            for (const auto& t : j["tags"]) manifest.tags.push_back(t.get<std::string>());
        }

        // Config object
        if (j.contains("config") && j["config"].is_object()) {
            for (auto it = j["config"].begin(); it != j["config"].end(); ++it) {
                // Coerce primitive values to string for simplicity
                if (it.value().is_string()) manifest.config[it.key()] = it.value().get<std::string>();
                else if (it.value().is_number_integer()) manifest.config[it.key()] = std::to_string(it.value().get<long long>());
                else if (it.value().is_number_float()) manifest.config[it.key()] = std::to_string(it.value().get<double>());
                else if (it.value().is_boolean()) manifest.config[it.key()] = it.value().get<bool>() ? "true" : "false";
                else manifest.config[it.key()] = it.value().dump();
            }
        }

        // Entry points
        if (j.contains("entry_points") && j["entry_points"].is_object()) {
            const auto& ep = j["entry_points"];
            if (ep.contains("init")) manifest.entry_points.init = ep.at("init").get<std::string>();
            if (ep.contains("start")) manifest.entry_points.start = ep.at("start").get<std::string>();
            if (ep.contains("stop")) manifest.entry_points.stop = ep.at("stop").get<std::string>();
            if (ep.contains("destroy")) manifest.entry_points.destroy = ep.at("destroy").get<std::string>();
        }
    } catch (const std::exception& e) {
        set_error(std::string("JSON parsing error: ") + e.what());
        return false;
    }
    return validate_manifest(manifest);
#else
    // Fallback: existing simplified/regex-based parser
    last_error_.clear();
    try {
        // existing code path retained below
        std::unordered_map<std::string, std::string> fields;
        {
            bool in_string = false;
            int obj_depth = 0;
            size_t i = 0;
            while (i < json_content.size() && json_content[i] != '{') ++i;
            if (i < json_content.size()) ++obj_depth, ++i;
            auto read_string = [&](size_t& idx) -> std::string {
                std::string out;
                ++idx;
                bool esc = false;
                for (; idx < json_content.size(); ++idx) {
                    char c = json_content[idx];
                    if (esc) { out.push_back(c); esc = false; continue; }
                    if (c == '\\') { esc = true; continue; }
                    if (c == '"') { ++idx; break; }
                    out.push_back(c);
                }
                return out;
            };
            auto skip_ws = [&](size_t& idx){ while (idx < json_content.size() && (json_content[idx]==' '||json_content[idx]=='\n'||json_content[idx]=='\r'||json_content[idx]=='\t')) ++idx; };
            for (; i < json_content.size() && obj_depth > 0; ++i) {
                char c = json_content[i];
                if (c == '"') {
                    size_t key_pos = i;
                    std::string key = read_string(i);
                    size_t j = i; skip_ws(j);
                    if (j < json_content.size() && json_content[j] == ':') {
                        ++j; skip_ws(j);
                        if (j < json_content.size() && json_content[j] == '"' && obj_depth == 1) {
                            std::string val = read_string(j);
                            fields[key] = val;
                            i = j - 1;
                            continue;
                        }
                    }
                    i = key_pos;
                } else if (c == '{') {
                    ++obj_depth;
                } else if (c == '}') {
                    --obj_depth;
                }
            }
        }

        if (fields.find("name") != fields.end()) manifest.name = fields["name"]; else { set_error("Missing required field: name"); return false; }
        if (fields.find("version") != fields.end()) manifest.version = fields["version"]; else { set_error("Missing required field: version"); return false; }
        if (fields.find("binary_path") != fields.end()) manifest.binary_path = fields["binary_path"]; else { set_error("Missing required field: binary_path"); return false; }

        manifest.description = fields.count("description") ? fields["description"] : "";
        manifest.author = fields.count("author") ? fields["author"] : "";
        manifest.license = fields.count("license") ? fields["license"] : "";
        manifest.minimum_api_version = fields.count("minimum_api_version") ? fields["minimum_api_version"] : "";
        manifest.homepage = fields.count("homepage") ? fields["homepage"] : "";
        manifest.repository = fields.count("repository") ? fields["repository"] : "";
        manifest.minimum_core_version = fields.count("minimum_core_version") ? fields["minimum_core_version"] : "";

        std::regex deps_regex("\"dependencies\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        std::smatch deps_match;
        if (std::regex_search(json_content, deps_match, deps_regex)) {
            if (!parse_dependencies(deps_match[1].str(), manifest.dependencies)) return false;
        }
        std::regex tags_regex("\"tags\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        std::smatch tags_match;
        if (std::regex_search(json_content, tags_match, tags_regex)) {
            if (!parse_string_array(tags_match[1].str(), manifest.tags)) return false;
        }
        std::regex config_regex("\"config\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
        std::smatch config_match;
        if (std::regex_search(json_content, config_match, config_regex)) {
            if (!parse_config(config_match[1].str(), manifest.config)) return false;
        }
        std::regex ep_obj_regex("\"entry_points\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
        std::smatch ep_obj_match;
        if (std::regex_search(json_content, ep_obj_match, ep_obj_regex)) {
            const std::string ep_json = ep_obj_match[1].str();
            std::regex ep_field_regex("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
            std::sregex_iterator eiter(ep_json.begin(), ep_json.end(), ep_field_regex), eend;
            for (; eiter != eend; ++eiter) {
                const std::string key = (*eiter)[1].str();
                const std::string val = (*eiter)[2].str();
                if (key == "init") manifest.entry_points.init = val;
                else if (key == "start") manifest.entry_points.start = val;
                else if (key == "stop") manifest.entry_points.stop = val;
                else if (key == "destroy") manifest.entry_points.destroy = val;
            }
        }
    } catch (const std::exception& e) {
        set_error("JSON parsing error: " + std::string(e.what()));
        return false;
    }
    return validate_manifest(manifest);
#endif
}

bool ManifestParser::validate_manifest(const ModuleManifest& manifest) {
    // Validate module name
    if (!is_valid_module_name(manifest.name)) {
        set_error("Invalid module name: " + manifest.name);
        return false;
    }

    // Validate version
    if (!is_valid_version(manifest.version)) {
        set_error("Invalid version format: " + manifest.version);
        return false;
    }

    // Validate minimum core version (if provided)
    if (!manifest.minimum_core_version.empty() && !is_valid_version(manifest.minimum_core_version)) {
        set_error("Invalid minimum_core_version format: " + manifest.minimum_core_version);
        return false;
    }

    // Validate minimum API version (if provided)
    if (!manifest.minimum_api_version.empty() && !is_valid_version(manifest.minimum_api_version)) {
        set_error("Invalid minimum_api_version format: " + manifest.minimum_api_version);
        return false;
    }

    // Validate binary path is not empty
    if (manifest.binary_path.empty()) {
        set_error("Binary path cannot be empty");
        return false;
    }

    // Validate dependencies
    for (const auto& dep : manifest.dependencies) {
        if (!is_valid_module_name(dep.name)) {
            set_error("Invalid dependency name: " + dep.name);
            return false;
        }
        if (!dep.version.empty() && !is_valid_version_requirement(dep.version)) {
            set_error("Invalid dependency version: " + dep.version);
            return false;
        }
    }

    // Validate entry point symbol names (must be valid identifiers)
    if (!manifest.entry_points.init.empty() && !is_valid_symbol_name(manifest.entry_points.init)) {
        set_error("Invalid entry point symbol for init: " + manifest.entry_points.init);
        return false;
    }
    if (!manifest.entry_points.start.empty() && !is_valid_symbol_name(manifest.entry_points.start)) {
        set_error("Invalid entry point symbol for start: " + manifest.entry_points.start);
        return false;
    }
    if (!manifest.entry_points.stop.empty() && !is_valid_symbol_name(manifest.entry_points.stop)) {
        set_error("Invalid entry point symbol for stop: " + manifest.entry_points.stop);
        return false;
    }
    if (!manifest.entry_points.destroy.empty() && !is_valid_symbol_name(manifest.entry_points.destroy)) {
        set_error("Invalid entry point symbol for destroy: " + manifest.entry_points.destroy);
        return false;
    }

    return true;
}

bool ManifestParser::is_valid_version(const std::string& version) {
    // Basic semantic version validation (X.Y.Z format)
    std::regex version_regex("^\\d+\\.\\d+\\.\\d+([+-][a-zA-Z0-9\\.-]*)?$");
    return std::regex_match(version, version_regex);
}

bool ManifestParser::is_valid_version_requirement(const std::string& requirement) {
    // Accept bare versions or operators >=, <=, >, <, ~, == followed by a semver
    // Allow optional pre-release/build suffix after patch
    std::regex req_regex(R"(^(>=|<=|>|<|~|==)?\s*\d+\.\d+\.\d+([+-][A-Za-z0-9\.-]+)?$)");
    return std::regex_match(requirement, req_regex);
}

bool ManifestParser::is_valid_module_name(const std::string& name) {
    // Module names should be alphanumeric with hyphens and underscores
    if (name.empty() || name.length() > 64) {
        return false;
    }
    
    std::regex name_regex("^[a-zA-Z][a-zA-Z0-9_-]*$");
    return std::regex_match(name, name_regex);
}

bool ManifestParser::is_valid_symbol_name(const std::string& symbol) {
    // C identifier: starts with letter or underscore, then letters/digits/underscore, allow namespace-like '::' not allowed for dlsym, so restrict to C-style
    if (symbol.empty() || symbol.size() > 128) return false;
    std::regex sym_regex("^[A-Za-z_][A-Za-z0-9_]*$");
    return std::regex_match(symbol, sym_regex);
}

std::string ManifestParser::serialize_manifest(const ModuleManifest& manifest) {
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << manifest.name << "\",\n";
    json << "  \"version\": \"" << manifest.version << "\",\n";
    json << "  \"description\": \"" << manifest.description << "\",\n";
    json << "  \"author\": \"" << manifest.author << "\",\n";
    json << "  \"license\": \"" << manifest.license << "\",\n";
    json << "  \"binary_path\": \"" << manifest.binary_path << "\",\n";
    // Entry points
    json << "  \"entry_points\": {\n";
    json << "    \"init\": \"" << manifest.entry_points.init << "\",\n";
    json << "    \"start\": \"" << manifest.entry_points.start << "\",\n";
    json << "    \"stop\": \"" << manifest.entry_points.stop << "\",\n";
    json << "    \"destroy\": \"" << manifest.entry_points.destroy << "\"\n";
    json << "  },\n";
    
    // Dependencies
    json << "  \"dependencies\": [\n";
    for (size_t i = 0; i < manifest.dependencies.size(); ++i) {
        const auto& dep = manifest.dependencies[i];
        json << "    {\n";
        json << "      \"name\": \"" << dep.name << "\",\n";
        json << "      \"version\": \"" << dep.version << "\",\n";
        json << "      \"optional\": " << (dep.optional ? "true" : "false") << "\n";
        json << "    }";
        if (i < manifest.dependencies.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";
    
    
    // Tags
    json << "  \"tags\": [";
    for (size_t i = 0; i < manifest.tags.size(); ++i) {
        json << "\"" << manifest.tags[i] << "\"";
        if (i < manifest.tags.size() - 1) json << ", ";
    }
    json << "],\n";
    
    // Config
    json << "  \"config\": {\n";
    size_t config_count = 0;
    for (const auto& [key, value] : manifest.config) {
        json << "    \"" << key << "\": \"" << value << "\"";
        if (++config_count < manifest.config.size()) json << ",";
        json << "\n";
    }
    json << "  },\n";
    
    if (!manifest.minimum_core_version.empty()) {
        json << "  \"minimum_core_version\": \"" << manifest.minimum_core_version << "\",\n";
    }
    if (!manifest.minimum_api_version.empty()) {
        json << "  \"minimum_api_version\": \"" << manifest.minimum_api_version << "\",\n";
    }

    json << "  \"homepage\": \"" << manifest.homepage << "\",\n";
    json << "  \"repository\": \"" << manifest.repository << "\"\n";
    json << "}";
    
    return json.str();
}

bool ManifestParser::parse_dependencies(const std::string& deps_json, std::vector<Dependency>& dependencies) {
    // Simplified dependency parsing
    if (deps_json.empty()) {
        return true;
    }

    // Parse each dependency object (very basic implementation)
    std::regex dep_regex("\\{[^}]*\"name\"\\s*:\\s*\"([^\"]+)\"[^}]*\"version\"\\s*:\\s*\"([^\"]*)\"[^}]*\"optional\"\\s*:\\s*(true|false)[^}]*\\}");
    std::sregex_iterator iter(deps_json.begin(), deps_json.end(), dep_regex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        Dependency dep;
        dep.name = (*iter)[1].str();
        dep.version = (*iter)[2].str();
        dep.optional = (*iter)[3].str() == "true";
        dependencies.push_back(dep);
    }

    return true;
}

bool ManifestParser::parse_config(const std::string& config_json, std::unordered_map<std::string, std::string>& config) {
    if (config_json.empty()) {
        return true;
    }

    std::regex config_regex("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
    std::sregex_iterator iter(config_json.begin(), config_json.end(), config_regex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        config[(*iter)[1].str()] = (*iter)[2].str();
    }

    return true;
}

bool ManifestParser::parse_string_array(const std::string& array_json, std::vector<std::string>& output) {
    if (array_json.empty()) {
        return true;
    }

    std::regex string_regex("\"([^\"]*)\"");
    std::sregex_iterator iter(array_json.begin(), array_json.end(), string_regex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        output.push_back((*iter)[1].str());
    }

    return true;
}

void ManifestParser::set_error(const std::string& error) {
    last_error_ = error;
    std::cerr << "Manifest error: " << error << std::endl;
}

} // namespace helix