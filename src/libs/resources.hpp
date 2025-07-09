#pragma once
const char* _env =
R"(#Database info
DB_HOST=localhost
DB_PORT=3306
DB_USER=root
DB_PASS=database

# If true, the app will use the default request handler. Otherwise, it will generate a new .cpp file with a function that you need to complete as needed. 
DEFAULT_REQUEST_HANDLER=true
# Primitive routing. Part of default request handler. If true, get requests to a url such us your.host.com/example will return the file ./public/example.html
HTML_ROUTING=true

# Debug mode not implemented yet
DEBUG_MODE=false)";

const char* plugin_hpp = R"(#ifndef PLUGIN_HPP
#define PLUGIN_HPP

#ifdef _WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

#include <memory>
#include <string>
#include <vector>

#include "request.hpp"

class ILogger {
   public:
    virtual void log(const std::string&) = 0;
    virtual void warning(const std::string&) = 0;
    virtual void error(const std::string&) = 0;
    virtual ~ILogger() = default;
};

using VALUE = void*;
using DOCUMENT = VALUE;
// I strongly reccommend to use another JSON lib, if you need a larger functionality!
// This IJson class was implemented above the rapidjson lib. It was added only in order to have a default JSON parser directly in the app.
// It was built on void* which implies both lack of stability and complex of code.
// If you wish you could add another lib into /app/headers and then include them into your plugins.
// E.g. #include "rapidjson-master/include/rapidjson/document.h"
class IJson {
   public:
    class IDocument {
       public:
        virtual DOCUMENT getDocument() = 0;
        virtual ~IDocument() = default;
    };

    virtual std::shared_ptr<IDocument> createDocumet() const = 0;
    virtual std::shared_ptr<IDocument> parse(std::string raw_json) = 0;

    virtual void setString(std::shared_ptr<IDocument> root, VALUE json, std::string value) = 0;
    virtual void setObject(std::shared_ptr<IDocument> root, VALUE json, VALUE json_object) = 0;
    virtual void setArray(std::shared_ptr<IDocument> root, VALUE json, std::vector<VALUE> json_array) = 0;

    virtual bool existsKey(VALUE json, std::string str) = 0;
    virtual VALUE getValueByKey(VALUE json, std::string key) = 0;
    virtual VALUE createKey(std::shared_ptr<IDocument> root, VALUE json_object, std::string key) = 0;

    virtual std::string getStringVal(VALUE json) const = 0;
    virtual std::vector<VALUE> getArrayVal(VALUE json) = 0;

    virtual std::string stringify(VALUE json) const = 0;

    virtual ~IJson() = default;
};

class IPlugin {
   public:
    virtual Response handle(Request) = 0;
    virtual bool isHeavy() = 0;

   protected:
    ILogger* _LOGGER_ = nullptr;
    IJson* _JSON_ = nullptr;
};

#endif)";

const char* request_hpp = R"(#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sstream>
#include <string>
#include <unordered_map>

class Request {
   public:
    std::string method;
    std::string uri;
    std::string http_version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    static Request parse(const std::string& raw_request) {
        Request req;
        std::istringstream stream(raw_request);
        std::string line;

        std::getline(stream, line);
        std::istringstream req_line(line);
        req_line >> req.method >> req.uri >> req.http_version;

        while (std::getline(stream, line) && line != "\r") {
            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                key.erase(key.find_last_not_of(" \r\n") + 1);
                value.erase(0, value.find_first_not_of(" "));
                value.erase(value.find_last_not_of(" \r\n") + 1);
                req.headers[key] = value;
            }
        }

        std::ostringstream body_stream;
        body_stream << stream.rdbuf();
        req.body = body_stream.str();

        return req;
    }
};

class Response {
   public:
    Response(int statusCode = 200, const std::string& statusMessage = "OK")
        : statusCode_(statusCode), statusMessage_(statusMessage) {
        setHeader("Content-Type", "text/plain");
    }

    void setStatus(int code, const std::string& message = "") {
        statusCode_ = code;
        statusMessage_ = message.empty() ? getDefaultStatusMessage(code) : message;
    }

    void setHeader(const std::string& name, const std::string& value) {
        headers_[name] = value;
    }

    void setContentType(const std::string& type) {
        setHeader("Content-Type", type);
    }
    void setContentEncoding(const std::string& encoding) {
        setHeader("Content-Encoding", encoding);
    }
    void setCacheControl(const std::string& value) {
        setHeader("Cache-Control", value);
    }
    void setETag(const std::string& etag) {
        setHeader("ETag", etag);
    }
    void setLastModified(const std::string& date) {
        setHeader("Last-Modified", date);
    }
    void setExpires(const std::string& date) {
        setHeader("Expires", date);
    }
    void setLocation(const std::string& url) {
        setHeader("Location", url);
    }
    void setContentSecurityPolicy(const std::string& policy) {
        setHeader("Content-Security-Policy", policy);
    }
    void setXFrameOptions(const std::string& option) {
        setHeader("X-Frame-Options", option);
    }
    void setXContentTypeOptions(const std::string& option = "nosniff") {
        setHeader("X-Content-Type-Options", option);
    }
    void setStrictTransportSecurity(const std::string& value) {
        setHeader("Strict-Transport-Security", value);
    }
    void setAccessControlAllowOrigin(const std::string& origin) {
        setHeader("Access-Control-Allow-Origin", origin);
    }
    void setAccessControlAllowMethods(const std::string& methods) {
        setHeader("Access-Control-Allow-Methods", methods);
    }
    void setAccessControlAllowHeaders(const std::string& headers) {
        setHeader("Access-Control-Allow-Headers", headers);
    }
    void setAccessControlExposeHeaders(const std::string& headers) {
        setHeader("Access-Control-Expose-Headers", headers);
    }
    void setConnection(const std::string& value) {
        setHeader("Connection", value);
    }
    void setKeepAlive(const std::string& value) {
        setHeader("Keep-Alive", value);
    }

    void setBody(const std::string& body) {
        body_ = body;
        headers_["Content-Length"] = std::to_string(body.size());
    }

    std::string toString() const {
        std::ostringstream stream;

        stream << "HTTP/1.1 " << statusCode_ << " " << statusMessage_ << "\r\n";

        for (const auto& [name, value] : headers_) {
            stream << name << ": " << value << "\r\n";
        }

        stream << "\r\n";

        stream << body_;

        return stream.str();
    }

    void clear() {
        headers_.clear();
        body_.clear();
    }

   private:
    std::string getDefaultStatusMessage(int code) const {
        switch (code) {
            case 200:
                return "OK";
            case 201:
                return "Created";
            case 204:
                return "No Content";
            case 304:
                return "Not Modified";
            case 400:
                return "Bad Request";
            case 401:
                return "Unauthorized";
            case 404:
                return "Not Found";
            case 500:
                return "Internal Server Error";
            default:
                return "Unknown";
        }
    }

    int statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
#endif)";

const char* test_cpp = 
R"(#include <iostream>
#include <string>

#include "../headers/plugin.hpp"
#include "../headers/request.hpp"

class Plugin : public IPlugin {
   public:
    Response handle(Request request) override {
        auto root = _JSON_->parse(R"({"message": "Here you have a json Object"})\");

        std::string raw_json = _JSON_->getStringVal(root->getDocument());
        _LOGGER_->log(raw_json);

        std::string content = "<h1>" + raw_json + "</h1>";

        Response response;
        response.setBody(content);
        response.setHeader("Content-Type", "text/html");
        response.setHeader("Content-Length", std::to_string(content.length()));
        return response;
    }
    bool isHeavy() override {
        return false;
    }
};

PLUGIN_EXPORT IPlugin* create() {
    return new Plugin;
})";

const char* custom_default_request_handler_cpp =
    R"(#include "headers/request.hpp"
extern "C" {
    Response handle(Request req) {
        Response response;
        response.setHeader("Content-Type", "text/html");
        response.setBody("<h1>Hello World</h1>");
        return response;
    }
})";

const char* index_html = 
R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CRouter</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1>Hello World!</h1>
</body>
</html>)";

const char* style_css = 
R"(h1 {
    background-color: green;
})";

const char* _404_NOT_FOUND_HTML = 
R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>404 Not Found</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body {
      height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      background: linear-gradient(135deg, #f5f7fa, #c3cfe2);
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    .container {
      text-align: center;
      padding: 2rem;
      background: white;
      border-radius: 20px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
      max-width: 500px;
    }
    .error-code {
      font-size: 6rem;
      font-weight: bold;
      color: #4e54c8;
    }
    .message {
      font-size: 1.5rem;
      color: #333;
      margin-top: 1rem;
    }

    .description {
      color: #666;
      margin: 1rem 0 2rem;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="error-code">404</div>
    <div class="message">Oops! Page Not Found</div>
    <p class="description">The page you're looking for might have been removed or is temporarily unavailable.</p>
  </div>
</body>
</html>)";