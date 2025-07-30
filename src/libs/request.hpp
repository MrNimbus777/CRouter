#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sstream>
#include <string>
#include <unordered_map>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

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

    boost::beast::http::request<boost::beast::http::string_body> to_beast() const {
        using namespace boost::beast;
        using http_request = http::request<http::string_body>;

        http_request beast_req;

        if (method == "GET") beast_req.method(http::verb::get);
        else if (method == "POST") beast_req.method(http::verb::post);
        else if (method == "PUT") beast_req.method(http::verb::put);
        else if (method == "DELETE") beast_req.method(http::verb::delete_);
        else beast_req.method(http::verb::unknown);

        beast_req.target(uri);

        if (http_version == "HTTP/1.0") beast_req.version(10);
        else if (http_version == "HTTP/1.1") beast_req.version(11);
        else beast_req.version(11);

        for (const auto& [key, value] : headers) {
            beast_req.set(key, value);
        }

        beast_req.body() = body;
        beast_req.prepare_payload();

        return beast_req;
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
        std::string headers_str;

        headers_str += "HTTP/1.1 " + std::to_string(statusCode_) + " " + statusMessage_ + "\r\n";

        for (const auto& [name, value] : headers_) {
            headers_str += name + ": " + value + "\r\n";
        }

        headers_str += "\r\n";

        std::string full_response = headers_str;
        full_response.append(body_);

        return full_response;
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
#endif