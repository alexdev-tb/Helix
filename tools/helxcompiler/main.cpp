#include "compiler.h"
#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* program_name) {
    std::cout << "Helix Module Compiler (helxcompiler)\n";
    std::cout << "Usage: " << program_name << " [options] <source_directory>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>     Output .helx file (default: <module_name>.helx)\n";
    std::cout << "  -n, --name <name>       Module name (auto-detected if not specified)\n";
    std::cout << "  -I, --include <path>    Add include directory\n";
    std::cout << "  -L, --library-path <path> Add library search path\n";
    std::cout << "  -l, --library <lib>     Link against library\n";
    std::cout << "  --std <standard>        C++ standard (default: c++17)\n";
    std::cout << "  -O, --optimize <level>  Optimization level (default: -O2)\n";
    std::cout << "  -g, --debug             Include debug information\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "  -h, --help              Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " my_module_src/\n";
    std::cout << "  " << program_name << " -o my_module.helx -v src/\n";
    std::cout << "  " << program_name << " --std c++20 -O3 -g module_dir/\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    helix::CompileConfig config;
    config.cxx_standard = "c++17";
    config.optimization_level = "-O2";
    config.debug_info = false;
    config.verbose = false;

    std::string source_directory;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "-o" || arg == "--output") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.output_file = argv[i];
        }
        else if (arg == "-n" || arg == "--name") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.module_name = argv[i];
        }
        else if (arg == "-I" || arg == "--include") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.include_paths.push_back(argv[i]);
        }
        else if (arg == "-L" || arg == "--library-path") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.library_paths.push_back(argv[i]);
        }
        else if (arg == "-l" || arg == "--library") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.libraries.push_back(argv[i]);
        }
        else if (arg == "--std") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.cxx_standard = argv[i];
        }
        else if (arg == "-O" || arg == "--optimize") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            config.optimization_level = "-O" + std::string(argv[i]);
        }
        else if (arg == "-g" || arg == "--debug") {
            config.debug_info = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return 1;
        }
        else {
            // This should be the source directory
            if (!source_directory.empty()) {
                std::cerr << "Error: Multiple source directories specified" << std::endl;
                return 1;
            }
            source_directory = arg;
        }
    }

    if (source_directory.empty()) {
        std::cerr << "Error: No source directory specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Create compiler instance
    helix::HelixCompiler compiler;

    // Auto-detect module configuration if needed
    if (config.module_name.empty() || config.output_file.empty()) {
        if (!compiler.detect_module_config(source_directory, config)) {
            std::cerr << "Error: " << compiler.get_last_error() << std::endl;
            return 1;
        }
    }

    // Set source directory
    config.source_directory = source_directory;

    // Set default output file if still empty
    if (config.output_file.empty()) {
        config.output_file = config.module_name + ".helx";
    }

    if (config.verbose) {
        std::cout << "Helix Module Compiler" << std::endl;
        std::cout << "Module name: " << config.module_name << std::endl;
        std::cout << "Source directory: " << config.source_directory << std::endl;
        std::cout << "Output file: " << config.output_file << std::endl;
        std::cout << "C++ standard: " << config.cxx_standard << std::endl;
        std::cout << "Optimization: " << config.optimization_level << std::endl;
        std::cout << "Debug info: " << (config.debug_info ? "yes" : "no") << std::endl;
        std::cout << std::endl;
    }

    // Compile the module
    if (!compiler.compile_module(config)) {
        std::cerr << "Compilation failed: " << compiler.get_last_error() << std::endl;
        return 1;
    }

    std::cout << "Successfully compiled " << config.module_name << " to " << config.output_file << std::endl;
    return 0;
}