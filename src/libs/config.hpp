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

    // Database
    std::string db_host = "localhost";
    int db_port = 3306;
    std::string db_user = "root";
    std::string db_pass = "defaultpass";
    
    // Plugin Loader
    bool default_request_handler = true;
    bool html_routing = true;


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

void loadConfig(Config& config, const std::string& filename) {
    const std::unordered_map<std::string, std::string> env = env_parser::parseEnvFile(filename);
    if (env.count("SERVER_PORT"))
        config.db_port = std::stoi(env.at("SERVER_PORT"));
    if(env.count("DB_HOST")) 
        config.db_host = env.at("DB_HOST");
    if(env.count("DB_PORT")) 
        config.db_port = std::stoi(env.at("DB_PORT"));
    if(env.count("DB_USER")) 
        config.db_user = env.at("DB_USER");
    if(env.count("DB_PASS")) 
        config.db_pass = env.at("DB_PASS");
    if(env.count("DEBUG_MODE")) 
        config.debug_mode = (env.at("DEBUG_MODE") == "true" || env.at("DEBUG_MODE") == "1");
    if(env.count("DEFAULT_REQUEST_HANDLER")) 
        config.default_request_handler = 
            (env.at("DEFAULT_REQUEST_HANDLER") == "true" || env.at("DEFAULT_REQUEST_HANDLER") == "1");
    if(env.count("HTML_ROUTING")) 
        config.html_routing = 
            (env.at("HTML_ROUTING") == "true" || env.at("HTML_ROUTING") == "1");
}

#endif