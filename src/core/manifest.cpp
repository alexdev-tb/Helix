#include "helix/manifest.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

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
    // NOTE: This is a simplified JSON parser for demonstration.
    // In a production system, you'd want to use a proper JSON library like nlohmann/json
    
    last_error_.clear();
    
    try {
        // Basic JSON parsing using regex (simplified approach)
    // Extract string fields
        std::regex string_field_regex("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
        std::sregex_iterator iter(json_content.begin(), json_content.end(), string_field_regex);
        std::sregex_iterator end;

        std::unordered_map<std::string, std::string> fields;
        for (; iter != end; ++iter) {
            fields[(*iter)[1].str()] = (*iter)[2].str();
        }

        // Extract required fields
        if (fields.find("name") != fields.end()) {
            manifest.name = fields["name"];
        } else {
            set_error("Missing required field: name");
            return false;
        }

        if (fields.find("version") != fields.end()) {
            manifest.version = fields["version"];
        } else {
            set_error("Missing required field: version");
            return false;
        }

        if (fields.find("binary_path") != fields.end()) {
            manifest.binary_path = fields["binary_path"];
        } else {
            set_error("Missing required field: binary_path");
            return false;
        }

        // Extract optional fields
        manifest.description = fields.count("description") ? fields["description"] : "";
        manifest.author = fields.count("author") ? fields["author"] : "";
        manifest.license = fields.count("license") ? fields["license"] : "";
        manifest.minimum_api_version = fields.count("minimum_api_version") ? fields["minimum_api_version"] : "";
        manifest.homepage = fields.count("homepage") ? fields["homepage"] : "";
        manifest.repository = fields.count("repository") ? fields["repository"] : "";
        manifest.minimum_core_version = fields.count("minimum_core_version") ? fields["minimum_core_version"] : "";

        // Parse dependencies array (simplified)
    std::regex deps_regex("\"dependencies\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        std::smatch deps_match;
        if (std::regex_search(json_content, deps_match, deps_regex)) {
            if (!parse_dependencies(deps_match[1].str(), manifest.dependencies)) {
                return false;
            }
        }

        // Parse capabilities array
    std::regex caps_regex("\"capabilities\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        std::smatch caps_match;
        if (std::regex_search(json_content, caps_match, caps_regex)) {
            if (!parse_string_array(caps_match[1].str(), manifest.capabilities)) {
                return false;
            }
        }

        // Parse tags array
    std::regex tags_regex("\"tags\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
        std::smatch tags_match;
        if (std::regex_search(json_content, tags_match, tags_regex)) {
            if (!parse_string_array(tags_match[1].str(), manifest.tags)) {
                return false;
            }
        }

        // Parse config object (simplified)
    std::regex config_regex("\"config\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
        std::smatch config_match;
        if (std::regex_search(json_content, config_match, config_regex)) {
            if (!parse_config(config_match[1].str(), manifest.config)) {
                return false;
            }
        }

        // Parse entry_points object (optional)
    std::regex ep_obj_regex("\"entry_points\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
        std::smatch ep_obj_match;
        if (std::regex_search(json_content, ep_obj_match, ep_obj_regex)) {
            const std::string ep_json = ep_obj_match[1].str();
            std::regex ep_field_regex("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
            std::sregex_iterator eiter(ep_json.begin(), ep_json.end(), ep_field_regex);
            std::sregex_iterator eend;
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
        if (!dep.version.empty() && !is_valid_version(dep.version)) {
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
    
    // Capabilities
    json << "  \"capabilities\": [";
    for (size_t i = 0; i < manifest.capabilities.size(); ++i) {
        json << "\"" << manifest.capabilities[i] << "\"";
        if (i < manifest.capabilities.size() - 1) json << ", ";
    }
    json << "],\n";
    
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