#ifndef HELIX_DEPENDENCY_RESOLVER_H
#define HELIX_DEPENDENCY_RESOLVER_H

#include "helix/manifest.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace helix {

/**
 * @brief Result of dependency resolution
 */
struct ResolutionResult {
    std::vector<std::string> load_order;    ///< Modules in load order (dependencies first)
    std::vector<std::string> missing_deps;  ///< Dependencies that couldn't be resolved
    std::vector<std::string> circular_deps; ///< Modules involved in circular dependencies
    bool success;                           ///< Whether resolution was successful
};

/**
 * @brief Dependency resolver for Helix modules
 * 
 * Builds dependency graphs from module manifests and determines the correct
 * load order. Detects circular dependencies and missing dependencies.
 */
class DependencyResolver {
public:
    DependencyResolver();
    ~DependencyResolver() = default;

    /**
     * @brief Add a module manifest to the resolver
     * @param manifest Module manifest to add
     * @return true if added successfully, false if there are conflicts
     */
    bool add_module(const ModuleManifest& manifest);

    /**
     * @brief Remove a module from the resolver
     * @param module_name Name of the module to remove
     */
    void remove_module(const std::string& module_name);

    /**
     * @brief Clear all modules from the resolver
     */
    void clear();

    /**
     * @brief Resolve dependencies and determine load order
     * @param target_modules Modules to resolve (empty = all modules)
     * @return ResolutionResult with load order or error information
     */
    ResolutionResult resolve_dependencies(const std::vector<std::string>& target_modules = {});

    /**
     * @brief Check if a module exists in the resolver
     * @param module_name Name of the module to check
     * @return true if module exists, false otherwise
     */
    bool has_module(const std::string& module_name) const;

    /**
     * @brief Get manifest for a module
     * @param module_name Name of the module
     * @return Pointer to manifest if found, nullptr otherwise
     */
    const ModuleManifest* get_module_manifest(const std::string& module_name) const;

    /**
     * @brief Get all module names in the resolver
     * @return Vector of module names
     */
    std::vector<std::string> get_all_modules() const;

    /**
     * @brief Get direct dependencies of a module
     * @param module_name Name of the module
     * @return Vector of dependency names
     */
    std::vector<std::string> get_dependencies(const std::string& module_name) const;

    /**
     * @brief Get modules that depend on a given module
     * @param module_name Name of the module
     * @return Vector of dependent module names
     */
    std::vector<std::string> get_dependents(const std::string& module_name) const;

    /**
     * @brief Check if version requirement is satisfied
     * @param available_version Version that's available
     * @param required_version Version requirement (e.g., ">=1.0.0")
     * @return true if requirement is satisfied, false otherwise
     */
    static bool version_satisfies(const std::string& available_version, 
                                  const std::string& required_version);

private:
    std::unordered_map<std::string, ModuleManifest> modules_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependency_graph_;
    std::unordered_map<std::string, std::unordered_set<std::string>> reverse_graph_;

    /**
     * @brief Build the dependency graph from loaded modules
     */
    void build_dependency_graph();

    /**
     * @brief Perform topological sort on the dependency graph
     * @param target_modules Modules to include in sort
     * @param load_order Output load order
     * @return true if successful (no cycles), false if cycles detected
     */
    bool topological_sort(const std::vector<std::string>& target_modules,
                         std::vector<std::string>& load_order);

    /**
     * @brief Detect circular dependencies in the graph
     * @param target_modules Modules to check
     * @return Vector of modules involved in cycles
     */
    std::vector<std::string> detect_circular_dependencies(const std::vector<std::string>& target_modules);

    /**
     * @brief Find missing dependencies
     * @param target_modules Modules to check
     * @return Vector of missing dependency names
     */
    std::vector<std::string> find_missing_dependencies(const std::vector<std::string>& target_modules);

    /**
     * @brief Recursive helper for cycle detection (DFS)
     * @param node Current node being visited
     * @param visited Set of visited nodes
     * @param rec_stack Set of nodes in current recursion stack
     * @param cycle_nodes Output set of nodes in cycles
     * @return true if cycle detected, false otherwise
     */
    bool detect_cycle_dfs(const std::string& node,
                         std::unordered_set<std::string>& visited,
                         std::unordered_set<std::string>& rec_stack,
                         std::unordered_set<std::string>& cycle_nodes);

    /**
     * @brief Parse version requirement string
     * @param requirement Version requirement (e.g., ">=1.0.0", "1.2.3", "~1.0")
     * @param operator_out Output operator (>=, ==, ~, etc.)
     * @param version_out Output version string
     * @return true if parsing succeeded, false otherwise
     */
    static bool parse_version_requirement(const std::string& requirement,
                                        std::string& operator_out,
                                        std::string& version_out);

    /**
     * @brief Compare two semantic versions
     * @param version1 First version
     * @param version2 Second version
     * @return -1 if version1 < version2, 0 if equal, 1 if version1 > version2
     */
    static int compare_versions(const std::string& version1, const std::string& version2);

    /**
     * @brief Parse a semantic version string into components
     * @param version Version string
     * @param major Output major version
     * @param minor Output minor version
     * @param patch Output patch version
     * @return true if parsing succeeded, false otherwise
     */
    static bool parse_version_components(const std::string& version,
                                       int& major, int& minor, int& patch);
};

} // namespace helix

#endif // HELIX_DEPENDENCY_RESOLVER_H