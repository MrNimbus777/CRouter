
#pragma once

#include <string>
#include <unordered_map>
#include <sstream>

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

private:
    std::string getDefaultStatusMessage(int code) const {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default:  return "Unknown";
        }
    }

    int statusCode_;
    std::string statusMessage_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
            