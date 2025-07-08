#pragma once
#include "request.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <mime_type.h>
#include "resources.hpp"
#include "config.hpp"

namespace _default_req_handler{
Response func(Request req) {
    Response res;

    std::string path = req.uri;
    if (path == "/") {
        path = "/index.html";
    } else if (CONF.html_routing) {
        int path_len = path.length();
        int current_slash = 0;

        while (current_slash < path_len) {
            int next_slash = path.find("/", current_slash + 1);
            if (next_slash == std::string::npos) next_slash = path_len;

            std::string current_route = path.substr(0, next_slash);
            std::string potential_path = "./public" + current_route + ".html";

            if (std::filesystem::is_regular_file(potential_path)) {
                path = current_route + ".html";
                break;
            }

            current_slash = next_slash;
        }
    }
    if (req.method == "GET") {
        try {
            std::filesystem::path public_root = std::filesystem::canonical("./public");
            std::filesystem::path full_path = std::filesystem::canonical(public_root / std::filesystem::path(path).relative_path());

            if (full_path.string().find(public_root.string()) != 0) {
                Response res;
                res.setStatus(403, "Forbidden");
                res.setHeader("Content-Type", "text/html");
                res.setBody("<h1>403 Forbidden</h1>");
                return res;
            }
            
            if (!std::filesystem::is_regular_file(full_path)){
                full_path = full_path / "index.html";
            }

            std::ifstream file(full_path, std::ios::in | std::ios::binary);
            if (!file.is_open()) throw std::filesystem::filesystem_error("File not found", std::error_code());
            
            std::ostringstream contents;
            contents << file.rdbuf();
            std::string content = contents.str();

            res.setHeader("Content-Type", MimeTypes::getType(full_path.extension().string().c_str()));
            res.setBody(content);
        } catch (const std::filesystem::filesystem_error& e) {
            Response res;
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/html");
            res.setBody(_404_NOT_FOUND_HTML);
            return res;
        }
    } else {
        res.setStatus(405, "Method Not Allowed");
        res.setHeader("Allow", "GET");
        res.setBody("Method Not Allowed");
    }

    return res;
}
};