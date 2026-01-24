#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "plugin.hpp"


struct Config {
    // Server
    int port = 8080;
    
    // Plugin Loader
    bool default_request_handler = true;
    bool cache = true;
    int cache_size_kb = 65356;
    bool html_routing = true;

    //CUSTOM_DEFAULT_HANDLER
    std::string custom_default_handler = "none";

    //PLGUIN COMPILING COMMAND
    std::string cmp_command = R"(g++ -shared -fPIC -o %dllPath% %cppPath% -I"app/headers")";

    //Debugging
    bool debug_mode = false;
};

Config CONF;

namespace env_parser
{
std::unordered_map<std::string, std::string> parseEnvFile(const std::string& filename) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(filename);
    if (!file.is_open()) {
        _LOGGER_.warning("Could not open " + filename);
        return env;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim spaces (simple)
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        env[key] = value;
    }

    return env;
}   
}

template <typename T>
bool parse(T& val, const std::string& str_val) {
    std::istringstream iss(str_val);
    T temp;
    if (!(iss >> temp)) {
        return false;
    }
    val = temp;
    return true;
}
template <>
bool parse(bool& val, const std::string& str_val) {
    if (str_val == "true" || str_val == "1") {
        val = true;
        return true;
    } else if (str_val == "false" || str_val == "0") {
        val = false;
        return true;
    }
    return false;
}
template <>
bool parse<std::string>(std::string& val, const std::string& str_val) {
    val = str_val;
    return true;
}

template<typename T>
void parseEnv(T& val, const std::unordered_map<std::string, std::string>& env, const std::string& key){
    if(auto it = env.find(key); it != env.end()) parse(val, it->second);
}

void loadConfig(const std::string& filename) {
    Config& config = CONF;
    
    const std::unordered_map<std::string, std::string> env = env_parser::parseEnvFile(filename);
    
    parseEnv(config.port, env, "SERVER_PORT");
    
    parseEnv(config.default_request_handler, env, "DEFAULT_REQUEST_HANDLER");
    parseEnv(config.cache, env, "CACHE");
    parseEnv(config.cache_size_kb, env, "CACHE_SIZE_KB");
    parseEnv(config.html_routing, env, "HTML_ROUTING");

    parseEnv(config.custom_default_handler, env, "CUSTOM_DEFAULT_HANDLER");
    
    parseEnv(config.cmp_command, env, "CMP_COMMAND");

    parseEnv(config.cache, env, "DEBUG_MODE");
}

#endif