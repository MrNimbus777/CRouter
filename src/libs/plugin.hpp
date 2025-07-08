#ifndef PLUGIN_HPP
#define PLUGIN_HPP

#ifdef _WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>

#include "request.hpp"

class ILogger {
   public:
    virtual void log(const std::string &) = 0;
    virtual void warning(const std::string &) = 0;
    virtual void error(const std::string &) = 0;
    virtual ~ILogger() = default;
};

using VALUE = void *;
using DOCUMENT = VALUE;

class IJson {
   public:
    class IDocument {
       public:
        virtual DOCUMENT getDocument() = 0;
        virtual ~IDocument() = default;
    };

    virtual std::shared_ptr<IDocument> createDocument() const = 0;
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
    IPlugin *setLogger(ILogger *logger) {
        this->_LOGGER_ = logger;
        return this;
    }
    IPlugin *setJSON(IJson *json) {
        this->_JSON_ = json;
        return this;
    }

   protected:
    ILogger *_LOGGER_ = nullptr;
    IJson *_JSON_ = nullptr;
};

#include <ctime>

namespace __PLUGIN_HELPER__ {
inline std::string getTime() {
    std::time_t now = std::time(nullptr);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}
inline std::string replace_all(const std::string &str, const std::string &from, const std::string &to) {
    if (from.empty()) return str;

    std::string result = str;
    size_t pos = 0;

    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }

    return result;
}
}

class Logger : public ILogger {
   public:
    void log(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
    }
    void warning(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "] " << "\033[34m[WARNING] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
    }
    void error(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "] " << "\033[31m[ERROR] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
    }

   private:
    std::mutex mutex_;
};

Logger _LOGGER_;

class Json : public IJson {
   public:
    class Document : public IJson::IDocument {
       public:
        std::unique_ptr<rapidjson::Document> doc;
        DOCUMENT getDocument() override { return this->doc.get(); }
        void setDocument(std::unique_ptr<rapidjson::Document> doc) {
            if (doc) {
                this->doc = std::move(doc);
            }
        }
    };

    std::shared_ptr<IDocument> createDocument() const override {
        std::shared_ptr<Document> doc_ptr = std::make_shared<Document>();
        doc_ptr->setDocument(std::make_unique<rapidjson::Document>());
        return doc_ptr;
    }
    std::shared_ptr<IDocument> parse(std::string raw_json) override {
        std::shared_ptr<Document> doc_ptr = std::make_shared<Document>();
        std::unique_ptr<rapidjson::Document> doc = std::make_unique<rapidjson::Document>();
        doc->Parse(raw_json.c_str());
        if (doc->HasParseError())
            throw std::runtime_error("Parse error at pos " + std::to_string(doc->GetErrorOffset()) + ": " + rapidjson::GetParseError_En(doc->GetParseError()));
        doc_ptr->setDocument(std::move(doc));
        return doc_ptr;
    }

    void setString(std::shared_ptr<IDocument> root, VALUE json, std::string value) override {
        if (!root) throw std::runtime_error("std::shared_ptr<IDocument> root - cannot be null");
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value *v = (rapidjson::Value *)json;
        v->SetString(value.c_str(), static_cast<rapidjson::Document *>(root->getDocument())->GetAllocator());
    }
    void setObject(std::shared_ptr<IDocument> root, VALUE json, VALUE json_object) override {
        if (!root) throw std::runtime_error("std::shared_ptr<IDocument> root - cannot be null");
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        if (!json_object) throw std::runtime_error("VALUE json_object - cannot be null");
        rapidjson::Value *v = (rapidjson::Value *)json;
        v->SetObject();
        rapidjson::Value &o = *(rapidjson::Value *)json_object;
        if (!o.IsObject()) throw std::runtime_error("VALUE json_object - is not a json object");
        v->CopyFrom(o, static_cast<rapidjson::Document *>(root->getDocument())->GetAllocator());
    }
    void setArray(std::shared_ptr<IDocument> root, VALUE json, std::vector<VALUE> json_array) override {
        if (!root) throw std::runtime_error("std::shared_ptr<IDocument> root - cannot be null");
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value *v = (rapidjson::Value *)json;
        v->SetArray();
        rapidjson::Document::AllocatorType alloc = static_cast<rapidjson::Document *>(root->getDocument())->GetAllocator();
        for (auto o : json_array) {
            v->PushBack(*(rapidjson::Value *)o, alloc);
        }
    }

    bool existsKey(VALUE json, std::string str) override {
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value &v = *(rapidjson::Value *)json;
        if (!v.IsObject()) throw std::runtime_error("VALUE json - is not a json object");
        return v.HasMember(str.c_str());
    }
    VALUE getValueByKey(VALUE json, std::string key) override {
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value *v = (rapidjson::Value *)json;
        return &(*v)[key.c_str()];
    }
    VALUE createKey(std::shared_ptr<IDocument> root, VALUE json_object, std::string key) override {
        if (!root) throw std::runtime_error("std::shared_ptr<IDocument> root - cannot be null");
        if (!json_object) throw std::runtime_error("VALUE json_object - cannot be null");
        rapidjson::Value &v = *(rapidjson::Value *)json_object;
        if (!v.IsObject()) throw std::runtime_error("VALUE json_object - is not a json object");
        rapidjson::Document::AllocatorType alloc = static_cast<rapidjson::Document *>(root->getDocument())->GetAllocator();
        v.AddMember(
            rapidjson::Value(key.c_str(), alloc),
            rapidjson::Value(rapidjson::kNullType),
            alloc);
        return &v[key.c_str()];
    }

    std::string getStringVal(VALUE json) const override {
        std::string raw_json = stringify(json);
        return raw_json.substr(1, raw_json.length() - 2);
    }
    std::vector<VALUE> getArrayVal(VALUE json) override {
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value *v = (rapidjson::Value *)json;
        if (!v->IsArray()) throw std::runtime_error("VALUE json - is not a json array");
        std::vector<void *> vec;
        for (auto &val : v->GetArray()) {
            vec.push_back(&val);
        }
        return vec;
    }

    std::string stringify(VALUE json) const override {
        if (!json) throw std::runtime_error("VALUE json - cannot be null");
        rapidjson::Value &v = *(rapidjson::Value *)json;

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        v.Accept(writer);

        return buffer.GetString();
    }
};

Json _JSON_;

#endif