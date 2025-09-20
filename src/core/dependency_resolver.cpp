#include "helix/dependency_resolver.h"
#include <algorithm>
#include <queue>
#include <iostream>
#include <regex>
#include <sstream>

namespace helix {

DependencyResolver::DependencyResolver() {
}

bool DependencyResolver::add_module(const ModuleManifest& manifest) {
    // Check if module already exists
    if (modules_.find(manifest.name) != modules_.end()) {
        std::cerr << "Module '" << manifest.name << "' already exists in resolver" << std::endl;
        return false;
    }

    // Add the module
    modules_[manifest.name] = manifest;
    
    // Rebuild dependency graph
    build_dependency_graph();
    
    return true;
}

void DependencyResolver::remove_module(const std::string& module_name) {
    auto it = modules_.find(module_name);
    if (it != modules_.end()) {
        modules_.erase(it);
        build_dependency_graph();
    }
}

void DependencyResolver::clear() {
    modules_.clear();
    dependency_graph_.clear();
    reverse_graph_.clear();
}

ResolutionResult DependencyResolver::resolve_dependencies(const std::vector<std::string>& target_modules) {
    ResolutionResult result;
    result.success = false;

    // If no target modules specified, resolve all modules
    std::vector<std::string> targets = target_modules;
    if (targets.empty()) {
        for (const auto& [name, manifest] : modules_) {
            targets.push_back(name);
        }
    }

    // Find missing dependencies
    result.missing_deps = find_missing_dependencies(targets);
    if (!result.missing_deps.empty()) {
        std::cerr << "Missing dependencies found" << std::endl;
        return result;
    }

    // Detect circular dependencies
    result.circular_deps = detect_circular_dependencies(targets);
    if (!result.circular_deps.empty()) {
        std::cerr << "Circular dependencies detected" << std::endl;
        return result;
    }

    // Perform topological sort to get load order
    if (!topological_sort(targets, result.load_order)) {
        std::cerr << "Failed to determine load order" << std::endl;
        return result;
    }

    result.success = true;
    return result;
}

bool DependencyResolver::has_module(const std::string& module_name) const {
    return modules_.find(module_name) != modules_.end();
}

const ModuleManifest* DependencyResolver::get_module_manifest(const std::string& module_name) const {
    auto it = modules_.find(module_name);
    return (it != modules_.end()) ? &it->second : nullptr;
}

std::vector<std::string> DependencyResolver::get_all_modules() const {
    std::vector<std::string> module_names;
    module_names.reserve(modules_.size());
    
    for (const auto& [name, manifest] : modules_) {
        module_names.push_back(name);
    }
    
    return module_names;
}

std::vector<std::string> DependencyResolver::get_dependencies(const std::string& module_name) const {
    auto it = dependency_graph_.find(module_name);
    if (it == dependency_graph_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> DependencyResolver::get_dependents(const std::string& module_name) const {
    auto it = reverse_graph_.find(module_name);
    if (it == reverse_graph_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

bool DependencyResolver::version_satisfies(const std::string& available_version, 
                                          const std::string& required_version) {
    if (required_version.empty()) {
        return true; // No version requirement
    }

    std::string op, version;
    if (!parse_version_requirement(required_version, op, version)) {
        return false; // Invalid requirement format
    }

    int cmp = compare_versions(available_version, version);

    if (op == "==" || op.empty()) {
        return cmp == 0;
    } else if (op == ">=") {
        return cmp >= 0;
    } else if (op == ">") {
        return cmp > 0;
    } else if (op == "<=") {
        return cmp <= 0;
    } else if (op == "<") {
        return cmp < 0;
    } else if (op == "~") {
        // Tilde version: compatible within same minor version
        int avail_major, avail_minor, avail_patch;
        int req_major, req_minor, req_patch;
        
        if (!parse_version_components(available_version, avail_major, avail_minor, avail_patch) ||
            !parse_version_components(version, req_major, req_minor, req_patch)) {
            return false;
        }
        
        return (avail_major == req_major && avail_minor == req_minor && avail_patch >= req_patch);
    }

    return false;
}

void DependencyResolver::build_dependency_graph() {
    dependency_graph_.clear();
    reverse_graph_.clear();

    // Build forward and reverse dependency graphs
    for (const auto& [module_name, manifest] : modules_) {
        dependency_graph_[module_name] = {};
        
        for (const auto& dep : manifest.dependencies) {
            // Skip optional dependencies that aren't available
            if (dep.optional && modules_.find(dep.name) == modules_.end()) {
                continue;
            }
            
            // Add to forward graph
            dependency_graph_[module_name].insert(dep.name);
            
            // Add to reverse graph
            reverse_graph_[dep.name].insert(module_name);
        }
    }
}

bool DependencyResolver::topological_sort(const std::vector<std::string>& target_modules,
                                         std::vector<std::string>& load_order) {
    load_order.clear();

    // Build subset graph for target modules and their dependencies
    std::unordered_set<std::string> all_needed;
    std::queue<std::string> to_process;
    
    // Add target modules to processing queue
    for (const auto& module : target_modules) {
        if (modules_.find(module) != modules_.end()) {
            to_process.push(module);
            all_needed.insert(module);
        }
    }
    
    // Recursively add dependencies
    while (!to_process.empty()) {
        std::string current = to_process.front();
        to_process.pop();
        
        auto deps_it = dependency_graph_.find(current);
        if (deps_it != dependency_graph_.end()) {
            for (const auto& dep : deps_it->second) {
                if (all_needed.find(dep) == all_needed.end()) {
                    all_needed.insert(dep);
                    to_process.push(dep);
                }
            }
        }
    }

    // Calculate in-degrees as the number of dependencies each module has within the subset.
    // With edges represented as module -> dependency in dependency_graph_,
    // the correct dependency-first order is achieved by:
    //  - in_degree[module] = number of deps in subset
    //  - when removing a node U (a dependency), decrement in_degree of its dependents
    //    using reverse_graph_[U].
    std::unordered_map<std::string, int> in_degree;
    for (const auto& module : all_needed) {
        int deg = 0;
        auto deps_it = dependency_graph_.find(module);
        if (deps_it != dependency_graph_.end()) {
            for (const auto& dep : deps_it->second) {
                if (all_needed.find(dep) != all_needed.end()) {
                    ++deg;
                }
            }
        }
        in_degree[module] = deg;
    }

    // Kahn's algorithm for topological sort
    std::queue<std::string> zero_in_degree;
    for (const auto& [module, degree] : in_degree) {
        if (degree == 0) {
            zero_in_degree.push(module);
        }
    }

    while (!zero_in_degree.empty()) {
        std::string current = zero_in_degree.front();
        zero_in_degree.pop();
        load_order.push_back(current);

        // Decrement in-degree of dependents of 'current'
        auto rev_it = reverse_graph_.find(current);
        if (rev_it != reverse_graph_.end()) {
            for (const auto& dependent : rev_it->second) {
                if (all_needed.find(dependent) != all_needed.end()) {
                    if (--in_degree[dependent] == 0) {
                        zero_in_degree.push(dependent);
                    }
                }
            }
        }
    }

    // Check if all modules were processed (no cycles)
    return load_order.size() == all_needed.size();
}

std::vector<std::string> DependencyResolver::detect_circular_dependencies(const std::vector<std::string>& target_modules) {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> rec_stack;
    std::unordered_set<std::string> cycle_nodes;

    for (const auto& module : target_modules) {
        if (visited.find(module) == visited.end()) {
            detect_cycle_dfs(module, visited, rec_stack, cycle_nodes);
        }
    }

    return std::vector<std::string>(cycle_nodes.begin(), cycle_nodes.end());
}

std::vector<std::string> DependencyResolver::find_missing_dependencies(const std::vector<std::string>& target_modules) {
    std::unordered_set<std::string> missing;

    for (const auto& module_name : target_modules) {
        auto it = modules_.find(module_name);
        if (it == modules_.end()) {
            missing.insert(module_name);
            continue;
        }

        const auto& manifest = it->second;
        for (const auto& dep : manifest.dependencies) {
            if (!dep.optional && modules_.find(dep.name) == modules_.end()) {
                missing.insert(dep.name);
            }
        }
    }

    return std::vector<std::string>(missing.begin(), missing.end());
}

bool DependencyResolver::detect_cycle_dfs(const std::string& node,
                                         std::unordered_set<std::string>& visited,
                                         std::unordered_set<std::string>& rec_stack,
                                         std::unordered_set<std::string>& cycle_nodes) {
    visited.insert(node);
    rec_stack.insert(node);

    auto deps_it = dependency_graph_.find(node);
    if (deps_it != dependency_graph_.end()) {
        for (const auto& dep : deps_it->second) {
            if (rec_stack.find(dep) != rec_stack.end()) {
                // Cycle detected
                cycle_nodes.insert(node);
                cycle_nodes.insert(dep);
                return true;
            }
            
            if (visited.find(dep) == visited.end()) {
                if (detect_cycle_dfs(dep, visited, rec_stack, cycle_nodes)) {
                    cycle_nodes.insert(node);
                    return true;
                }
            }
        }
    }

    rec_stack.erase(node);
    return false;
}

bool DependencyResolver::parse_version_requirement(const std::string& requirement,
                                                  std::string& operator_out,
                                                  std::string& version_out) {
    std::regex req_regex(R"(^(>=|<=|>|<|~|==)?\s*(\d+\.\d+\.\d+.*)$)");
    std::smatch match;
    
    if (std::regex_match(requirement, match, req_regex)) {
        operator_out = match[1].str();
        version_out = match[2].str();
        return true;
    }
    
    return false;
}

int DependencyResolver::compare_versions(const std::string& version1, const std::string& version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    if (!parse_version_components(version1, major1, minor1, patch1) ||
        !parse_version_components(version2, major2, minor2, patch2)) {
        return 0; // Invalid versions are considered equal
    }
    
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    
    return 0; // Equal
}

bool DependencyResolver::parse_version_components(const std::string& version,
                                                 int& major, int& minor, int& patch) {
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+))");
    std::smatch match;
    
    if (std::regex_match(version, match, version_regex)) {
        major = std::stoi(match[1].str());
        minor = std::stoi(match[2].str());
        patch = std::stoi(match[3].str());
        return true;
    }
    
    return false;
}

} // namespace helix