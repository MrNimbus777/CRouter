#pragma once

#include <unordered_map>
#include <lib_wrapper.hpp>
#include "plugin.hpp"


namespace _PLUGINS_ {
    struct LibInstance{
        std::unique_ptr<LibWraper> lib = nullptr;
        std::shared_ptr<IPlugin> instance = nullptr;
        LibInstance(std::unique_ptr<LibWraper> l, IPlugin* p): lib(std::move(l)), instance(p) {}
    };
    std::unordered_map<std::string, LibInstance> loadedPlugins;
    void clear(){
        loadedPlugins.clear();
    }
    void loadPlugins(const std::string& dir){
        clear();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() != ".cpp") continue;
            try{
                auto lib = std::make_unique<LibWraper>(entry.path().string());
                auto lib_ptr = lib.get();
                loadedPlugins.emplace(
                    lib_ptr->getName(),
                    std::move(LibInstance{
                        std::move(lib),
                        lib_ptr->loadAndGetProcedure<IPlugin*()>("create")()
                        ->setLogger(&_LOGGER_)->setWSM(&_WEBSOCKETS_)
                    }));
                _LOGGER_.log("Loaded plugin: " + lib_ptr->getName());
            } catch(const std::exception& e){
                _LOGGER_.error("Failed to load " + entry.path().string() + ": " + e.what());
            }
        }
    }
    std::shared_ptr<IPlugin> getPlugin(const std::string& pluginName) {
        auto it = loadedPlugins.find(pluginName);
        if (it != loadedPlugins.end()) {
            return it->second.instance;
        }
        return nullptr;
    }
   
}