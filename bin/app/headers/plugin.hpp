#ifndef PLUGIN_HPP
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
// It was built on void* which implies both lack of safety and complex of code.
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

#endif