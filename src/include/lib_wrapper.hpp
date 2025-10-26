#pragma once

#include <string>
#include <functional>
#include <fstream>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include "plugin.hpp"

#ifdef _WIN32

#include <windows.h>

class LibWraper{
 public:
    explicit LibWraper(const std::string& path2source){
        std::filesystem::path entry;
        try {
            entry = std::filesystem::canonical(path2source);
        } catch (const std::filesystem::filesystem_error& e) {
            throw std::runtime_error("Invalid path: " + path2source + " (" + e.what() + ")");
        }

        if (entry.extension() != ".cpp") 
            throw std::runtime_error("Invalid path: " + path2source + " is not a .cpp");
        name = entry.stem().string();
        std::filesystem::path dllPath = entry.parent_path() / (name + ".dll");

        std::string compileCmd =
            "g++ -shared -fPIC -o \"" + dllPath.string()  + "\" \"" + entry.string() + "\" -I\"app/headers\"";
        
        std::ifstream file(entry.string());
        std::string line;

        if (file.is_open()) {
            std::getline(file, line);
            if(line.find("//cmp:") == 0) compileCmd.append(" ").append(line.substr(6));
            file.close();
        }
        

        _LOGGER_.log("Compiling " + entry.filename().string() + "...\n" + compileCmd);
        if (std::system(compileCmd.c_str()) != 0) {
            throw std::runtime_error("Compilation failed for: " + path2source);
        }
        path2lib = dllPath.string();

        HMODULE lib = LoadLibraryA(path2lib.c_str());
        if (!lib) {
            DWORD err = GetLastError();
            throw std::runtime_error(std::string("Error while loading " + path2lib));
        }
        this->lib_ = lib;
    }
    void loadProcedure(const std::string& func_name) {
        auto f = GetProcAddress(lib_, func_name.c_str());
        if (!f) {
            throw std::runtime_error("Function not found: " + func_name + "(...) in " + path2lib);
        }
        map_.emplace(func_name, (void*) f);
    }
    template<typename Signature>
    std::function<Signature> getLoadedProcedure(const std::string& func_name) {
        if(auto it = map_.find(func_name); it != map_.end()){
            return std::function<Signature>(*reinterpret_cast<Signature*>(it->second));
        }
        return nullptr;
    }
    template<typename Signature>
    std::function<Signature> loadAndGetProcedure(const std::string& func_name) {
        loadProcedure(func_name);
        return getLoadedProcedure<Signature>(func_name);
    }
    const std::string& getName() {
        return name;
    }
    ~LibWraper(){
        if(lib_) FreeLibrary(lib_);
    }
    LibWraper(const LibWraper&) = delete;
    LibWraper(LibWraper&&) = default;
 private:
    std::string name;
    std::string path2lib;
    HMODULE lib_ = nullptr;
    std::unordered_map<std::string, void*> map_;
};

#else

#include <dlfcn.h>
#include <cstdlib>

class LibWrapper {
public:
    explicit LibWrapper(const std::string& path2source) {
        namespace fs = std::filesystem;
        fs::path entry;

        try {
            entry = fs::canonical(path2source);
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Invalid path: " + path2source + " (" + e.what() + ")");
        }

        if (entry.extension() != ".cpp")
            throw std::runtime_error("Invalid path: " + path2source + " is not a .cpp");

        name = entry.stem().string();
        fs::path soPath = entry.parent_path() / ("lib" + name + ".so");

        std::string compileCmd =
            "g++ -shared -fPIC -o \"" + soPath.string() + "\" \"" + entry.string() + "\" -I\"app/headers\"";

        std::ifstream file(entry.string());
        std::string line;
        if (file.is_open()) {
            std::getline(file, line);
            if (line.find("//cmp:") == 0)
                compileCmd.append(" ").append(line.substr(6));
            file.close();
        }

        _LOGGER_.log("Compiling " + entry.filename().string() + "...\n" + compileCmd);

        if (std::system(compileCmd.c_str()) != 0) {
            throw std::runtime_error("Compilation failed for: " + path2source);
        }

        path2lib = soPath.string();

        void* lib = dlopen(path2lib.c_str(), RTLD_LAZY);
        if (!lib) {
            throw std::runtime_error("Error while loading " + path2lib + ": " + std::string(dlerror()));
        }

        this->lib_ = lib;
    }

    void loadProcedure(const std::string& func_name) {
        void* f = dlsym(lib_, func_name.c_str());
        if (!f) {
            throw std::runtime_error("Function not found: " + func_name + " in " + path2lib);
        }

        map_[func_name] = f;
    }

    template<typename Signature>
    std::function<Signature> getLoadedProcedure(const std::string& func_name) {
        if (auto it = map_.find(func_name); it != map_.end()) {
            return std::function<Signature>(*reinterpret_cast<Signature*>(it->second));
        }
        return nullptr;
    }

    template<typename Signature>
    std::function<Signature> loadAndGetProcedure(const std::string& func_name) {
        loadProcedure(func_name);
        return getLoadedProcedure<Signature>(func_name);
    }

    std::string& getName(){
        return name;
    }

    ~LibWrapper() {
        if (lib_) dlclose(lib_);
    }

private:
    std::string name;
    std::string path2lib;
    void* lib_ = nullptr;
    std::unordered_map<std::string, void*> map_;
};

#endif
