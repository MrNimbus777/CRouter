#include <string>
#include "../headers/request.hpp"
extern "C" {
    Response handle(Request request) {
        std::string content = "<h1>Test passed!</h1>";
        Response response;
        response.setBody(content);
        response.setHeader("Content-Type", "text/html");
        response.setHeader("Content-Length", std::to_string(content.length()));
        return response;
    }
}