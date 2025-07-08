#pragma once

#include <filesystem>
#include <iostream>
#include <fstream>
#include "resources.hpp"
#include "plugin.hpp"


void write_default_resources(){
    if (!std::filesystem::exists("./app/headers")) {
        if (std::filesystem::create_directories("./app/headers")) {
            std::cout << "Created directory: app/headers" << '\n';
            std::ofstream out("app/headers/request.hpp");
            out << request_hpp;
            out.close();
            out = std::ofstream("app/headers/plugin.hpp");
            out << plugin_hpp;
            out.close();
        } else {
            _LOGGER_.error("Failed to create directory: ./app/headers");
        }
    }
    if (!std::filesystem::exists("./app/handlers")) {
        if (std::filesystem::create_directories("./app/handlers")) {
            std::cout << "Created directory: app/requesthandlers" << '\n';
            std::ofstream out("app/handlers/test.cpp");
            out << test_cpp;
            out.close();
        } else {
            _LOGGER_.error("Failed to create directory: ./app/handlers");
        }
    }
    if (!std::filesystem::exists("./public")) {
        if (std::filesystem::create_directories("./public")) {
            std::cout << "Created directory: public" << '\n';
            std::ofstream out("./public/index.html");
            out << index_html;
            out.close();
            out = std::ofstream("./public/style.css");
            out << style_css;
            out.close();
        } else {
            _LOGGER_.error("Failed to create directory: ./public");
        }
    }
    if (!std::filesystem::exists("./.env")) {
        std::ofstream out("./.env");
        out << _env;
        out.close();
    }
}