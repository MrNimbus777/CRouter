#pragma once

#include <unordered_map>
#include "dynamic_compiler.hpp"
#include "plugin.hpp"


namespace _PLUGINS_ {
    std::unordered_map<std::string, IPlugin*> loadedPlugins;
    void loadPlugins(std::string dir){
        loadedPlugins.clear();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".cpp") {
                std::string libPath = compileFile(entry.path().string());
                if (libPath.empty()) continue;

                auto plugin = loadFunctions(libPath, {"create"});
                std::string name = entry.path().stem().string();
                auto it = plugin.functions_map.find("create");
                if (it != plugin.functions_map.end()) {
                    IPlugin* pl = ((IPlugin*(*)()) it->second)();
                    loadedPlugins[name] = pl->setLogger(&_LOGGER_)->setWSM(&_WEBSOCKETS_);
                    _LOGGER_.log("Loaded plugin: " + name);
                } else _LOGGER_.error("Failed to load " + name);
            }
        }
    }
    IPlugin* getPlugin(const std::string& pluginName) {
        auto it = loadedPlugins.find(pluginName);
        if (it != loadedPlugins.end()) {
            return it->second;
        }
        return nullptr;
    }
}