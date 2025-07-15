#pragma once
#include <mime_type.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "config.hpp"
#include "request.hpp"
#include "resources.hpp"

namespace _default_req_handler {
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
            if (path.empty() || path == "/") {
                path = "/index.html";
            }

            std::filesystem::path requested_path = std::filesystem::path(path).relative_path();
            std::filesystem::path full_path = std::filesystem::canonical(public_root / requested_path);

            if (full_path.string().find(public_root.string()) != 0) {
                res.setStatus(403, "Forbidden");
                res.setHeader("Content-Type", "text/html");
                res.setBody("<h1>403 Forbidden</h1>");
                return res;
            }

            if (std::filesystem::is_directory(full_path)) {
                full_path /= "index.html";
            }

            if (!std::filesystem::is_regular_file(full_path)) {
                throw std::filesystem::filesystem_error("File not found", std::error_code());
            }

            std::ifstream file(full_path, std::ios::in | std::ios::binary);
            if (!file.is_open()) {
                throw std::filesystem::filesystem_error("File not found or inaccessible", std::error_code());
            }

            res.setStatus(200, "OK");
            res.setContentType(MimeTypes::getType(full_path.extension().string().c_str()));

            file.seekg(0, std::ios::end);
            std::streampos file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string content;
            if (file_size > 0) {
                content.resize(static_cast<std::string::size_type>(file_size));
                file.read(&content[0], file_size);
            }
            res.setBody(content);
            res.setCacheControl("public, max-age=2678400");
            return res;

        } catch (const std::filesystem::filesystem_error& e) {
            _LOGGER_.warning("Filesystem error while serving: " + std::string(e.what()));
            res.setStatus(404, "Not Found");
            res.setContentType("text/html");
            res.setBody("<h1>404 Not Found</h1>");
            return res;
        } catch (const std::exception& e) {
            _LOGGER_.error("Unexpected error serving static file: " + std::string(e.what()));
            res.setStatus(500, "Internal Server Error");
            res.setContentType("text/html");
            res.setBody("<h1>500 Internal Server Error</h1>");
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