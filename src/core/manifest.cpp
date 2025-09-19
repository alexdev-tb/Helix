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
        manifest.api_version = fields.count("api_version") ? fields["api_version"] : "1.0.0";
        manifest.homepage = fields.count("homepage") ? fields["homepage"] : "";
        manifest.repository = fields.count("repository") ? fields["repository"] : "";

        // Parse dependencies array (simplified)
        std::regex deps_regex("\"dependencies\"\\s*:\\s*\\[(.*?)\\]");
        std::smatch deps_match;
        if (std::regex_search(json_content, deps_match, deps_regex)) {
            if (!parse_dependencies(deps_match[1].str(), manifest.dependencies)) {
                return false;
            }
        }

        // Parse capabilities array
        std::regex caps_regex("\"capabilities\"\\s*:\\s*\\[(.*?)\\]");
        std::smatch caps_match;
        if (std::regex_search(json_content, caps_match, caps_regex)) {
            if (!parse_string_array(caps_match[1].str(), manifest.capabilities)) {
                return false;
            }
        }

        // Parse tags array
        std::regex tags_regex("\"tags\"\\s*:\\s*\\[(.*?)\\]");
        std::smatch tags_match;
        if (std::regex_search(json_content, tags_match, tags_regex)) {
            if (!parse_string_array(tags_match[1].str(), manifest.tags)) {
                return false;
            }
        }

        // Parse config object (simplified)
        std::regex config_regex("\"config\"\\s*:\\s*\\{(.*?)\\}");
        std::smatch config_match;
        if (std::regex_search(json_content, config_match, config_regex)) {
            if (!parse_config(config_match[1].str(), manifest.config)) {
                return false;
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

    // Validate API version
    if (!manifest.api_version.empty() && !is_valid_version(manifest.api_version)) {
        set_error("Invalid API version format: " + manifest.api_version);
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

std::string ManifestParser::serialize_manifest(const ModuleManifest& manifest) {
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << manifest.name << "\",\n";
    json << "  \"version\": \"" << manifest.version << "\",\n";
    json << "  \"description\": \"" << manifest.description << "\",\n";
    json << "  \"author\": \"" << manifest.author << "\",\n";
    json << "  \"license\": \"" << manifest.license << "\",\n";
    json << "  \"binary_path\": \"" << manifest.binary_path << "\",\n";
    json << "  \"api_version\": \"" << manifest.api_version << "\",\n";
    
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