#include <iostream>
#include <string>

#include "../headers/plugin.hpp"
#include "../headers/request.hpp"

class Plugin : public IPlugin {
   public:
    Response handle(Request request) override {
        auto root = _JSON_->parse(R"({"message": "Here you have a json Object"})");
        
        std::string raw_json = _JSON_->getStringVal(root->getDocument());
        _LOGGER_->log(raw_json);
        
        std::string content = "<h1>"+raw_json+"</h1>";

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
}