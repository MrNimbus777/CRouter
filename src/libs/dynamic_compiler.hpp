#pragma once

#include <string>
#include <filesystem>
#include <iostream>

#include "plugin.hpp"

#ifdef _WIN32
#include <windows.h>

std::string compileFile(const std::string& path) {
    std::filesystem::path entry;
    try {
        entry = std::filesystem::canonical(path);
    } catch (const std::filesystem::filesystem_error& e) {
        _LOGGER_.error("Invalid path: " + path + " (" + e.what() + ")");
        return "";
    }

    if (entry.extension() == ".cpp") {
        std::string name = entry.stem().string();
        std::filesystem::path dllPath = entry.parent_path() / (name + ".dll");

        std::string compileCmd =
            "g++ -shared -fPIC -o \"" + dllPath.string()  + "\" \"" + entry.string() + "\"";
        
        _LOGGER_.log("Compiling " + entry.filename().string() + "...");
        if (std::system(compileCmd.c_str()) != 0) {
            _LOGGER_.error("Compilation failed for: " + path);
            return "";
        }
        return dllPath.string();
    }

    return "";
}

struct PluginInstance {
    std::unordered_map<std::string, void*> functions_map;
    HMODULE library;
};

PluginInstance loadFunctions(const std::string& dllPath, const std::vector<std::string> func_names) {
    PluginInstance result = { std::unordered_map<std::string, void*>(), nullptr };

    HMODULE lib = LoadLibraryA(dllPath.c_str());
    if (!lib) {
        _LOGGER_.error("Failed to load DLL: " + dllPath);
        return result;
    }
    result.library = lib;
    
    for(const auto& func_name : func_names) {
        auto f = GetProcAddress(lib, func_name.c_str());
        if (!f) {
            _LOGGER_.error("Function not found: " + func_name + " in " + dllPath);
            FreeLibrary(lib);
            return result;
        }

        result.functions_map[func_name] = (void*) f;
    }
    return result;
}

void freeLibrary(PluginInstance plugin){
    if(plugin.library) FreeLibrary(plugin.library);
}

#elif defined(__linux__)
#include <dlfcn.h>

std::string compileFile(const std::string& path) {
    std::filesystem::path entry;
    try {
        entry = std::filesystem::canonical(path);
    } catch (const std::filesystem::filesystem_error& e) {
        _LOGGER_.error("Invalid path: " + path + " (" + e.what() + ")");
        return "";
    }

    if (entry.extension() == ".cpp") {
        std::string name = entry.stem().string();
        std::filesystem::path soPath = entry.parent_path() / (name + ".so");

        std::string compileCmd =
            "g++ -shared -fPIC -o \"" + soPath.string()  + "\" \"" + entry.string() + "\"";

        _LOGGER_.log("Compiling " + entry.filename().string() + "...");
        if (std::system(compileCmd.c_str()) != 0) {
            _LOGGER_.error("Compilation failed for: " + path);
            return "";
        }
        return soPath.string();
    }

    return "";
}

struct PluginInstance {
    std::unordered_map<std::string, void*> functions_map;
    void* library = nullptr;
};

PluginInstance loadFunctions(const std::string& soPath, const std::vector<std::string>& func_names) {
    PluginInstance result;

    void* lib = dlopen(soPath.c_str(), RTLD_LAZY);
    if (!lib) {
        _LOGGER_.error("Failed to load shared object: " + soPath + "(" + dlerror() + ")");
        return result;
    }

    result.library = lib;

    for (const auto& func_name : func_names) {
        void* f = dlsym(lib, func_name.c_str());
        if (!f) {
            _LOGGER_.error("Function not found: " + func_name + "(" + dlerror() + ")");
            dlclose(lib);
            result.library = nullptr;
            return result;
        }

        result.functions_map[func_name] = f;
    }

    return result;
}

void freeLibrary(PluginInstance plugin) {
    if (plugin.library) {
        dlclose(plugin.library);
    }
}
#endif