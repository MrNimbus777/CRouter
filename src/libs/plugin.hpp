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

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <queue>

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
    virtual VALUE createNullValue() = 0;
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

class IWebSocket {
   public:
    virtual void send(const std::string &message) = 0;
    virtual void registerKey(const std::string &) = 0;
    virtual const std::string &getKey() = 0;
    virtual void setOnRecieve(std::function<void(const std::string &)>) = 0;
    virtual void setOnClose(std::function<void()>) = 0;
    virtual ~IWebSocket() = default;
};
class IWebSocketPool {
   public:
    virtual IWebSocket *getSocket(const std::string &key) = 0;
    virtual IWebSocket *make_from_request(Request &request) = 0;
    virtual void putSocket(const std::string &key, IWebSocket *ws) = 0;
    virtual void erase_and_close(const std::string &key) = 0;
    virtual ~IWebSocketPool() = default;
};

class IPlugin {
   public:
    virtual Response handle(Request &) = 0;
    virtual bool isHeavy() = 0;
    IPlugin *setLogger(ILogger *logger) {
        this->_LOGGER_ = logger;
        return this;
    }
    IPlugin *setJSON(IJson *json) {
        this->_JSON_ = json;
        return this;
    }
    IPlugin *setWSM(IWebSocketPool *websockets) {
        this->_WEBSOCKETS_ = websockets;
        return this;
    }
    virtual ~IPlugin() = default;

   protected:
    ILogger *_LOGGER_ = nullptr;
    IJson *_JSON_ = nullptr;
    IWebSocketPool *_WEBSOCKETS_ = nullptr;
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
}  // namespace __PLUGIN_HELPER__

class Logger : public ILogger {
   public:
    void log(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
    }
    void warning(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] \033[34m[WARNING] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
    }
    void error(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] \033[31m[ERROR] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n";
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
    VALUE createNullValue() override {
        return std::make_shared<rapidjson::Value>().get();
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

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>, public IWebSocket {
   public:
    WebSocketSession(
        std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>> ws,
        boost::asio::io_context &io_context,
        boost::asio::thread_pool &worker_pool)
        : ws_(ws),
          io_context_(io_context),
          worker_pool_(worker_pool),
          strand_(io_context) {}
    ~WebSocketSession();
    void start() {
        ws_->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

        ws_->set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type &res) {
                res.set(boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) + " serv-websocket-server");
            }));
        _LOGGER_.log("WebSocket session started.");
        do_read();
    }

    void send(const std::string &message) override {
        boost::asio::post(strand_, [self = shared_from_this(), message = std::move(message)]() mutable {
            bool write_in_progress = !self->write_queue_.empty();
            self->write_queue_.push(std::move(message));
            if (!write_in_progress) {
                self->do_write();
            }
        });
    }
    void registerKey(const std::string &key) override;
    const std::string &getKey() override {
        return this->key;
    }
    void setOnRecieve(std::function<void(const std::string&)> onrecieve) override {
        this->onrecieve = std::move(onrecieve);
    }
    void setOnClose(std::function<void()> onclose) override {
        this->onclose = std::move(onclose);
    }
    void close() {
        auto self = shared_from_this();
        ws_->async_close(boost::beast::websocket::close_code::normal,
            [self](boost::system::error_code ec) {
                if (ec) {
                    _LOGGER_.error("WebSocket close failed: " + ec.message());
                } else {
                    _LOGGER_.log("WebSocket closed successfully.");
                }
                if (self->onclose) self->onclose();
            });
    }

   private:
    void do_read() {
        auto self = shared_from_this();
        ws_->async_read(buffer_,
            boost::asio::bind_executor(strand_,
                [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (ec == boost::beast::websocket::error::closed) {
                        _LOGGER_.log("WebSocket connection closed.");
                        return;
                    }
                    if (ec) {
                        _LOGGER_.error("Error during WebSocket read: " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
                        return;
                    }

                    std::string message = boost::beast::buffers_to_string(self->buffer_.data());
                    self->buffer_.consume(self->buffer_.size());

                    _LOGGER_.log("WebSocket message received: " + message);

                    boost::asio::post(self->worker_pool_, [self, message]() {
                        if(self->onrecieve) self->onrecieve(message);
                    });

                    self->do_read();
                }));
    }

    void do_write() {
        if (write_queue_.empty()) {
            return;
        }

        ws_->async_write(boost::asio::buffer(write_queue_.front()),
            boost::asio::bind_executor(strand_,
                [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (ec) {
                        _LOGGER_.error("Error during WebSocket write: " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
                        return;
                    }
                    self->write_queue_.pop();
                    self->do_write();
                }));
    }
    friend class WebSocketPool;
    std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>> ws_;
    boost::asio::io_context &io_context_;
    boost::asio::thread_pool &worker_pool_;
    boost::asio::io_context::strand strand_;
    boost::beast::flat_buffer buffer_;
    std::queue<std::string> write_queue_;
    std::function<void(const std::string &)> onrecieve = nullptr;
    std::function<void()> onclose = nullptr;
    std::string key = "empty key";
};
namespace serv{
    class Session;
};
class WebSocketPool : public IWebSocketPool {
   private:
    std::unordered_map<std::string, IWebSocket *> map_;

   public:
    std::unordered_map<Request *, std::shared_ptr<serv::Session>> request_sockets;
    IWebSocket *getSocket(const std::string &key) {
        auto it = map_.find(key);
        if (it == map_.end()) return nullptr;
        return it->second;
    }
    IWebSocket *make_from_request(Request &req);

    void putSocket(const std::string &key, IWebSocket *ws) {
        map_[key] = ws;
    }
    void erase_and_close(const std::string &key) {
        auto it = map_.find(key);
        if (it == map_.end()) return;

        WebSocketSession *ptr = (WebSocketSession *)it->second;
        
        map_.erase(it);

        if(ptr) if(ptr->ws_->is_open()) ptr->close();
    }
};
WebSocketPool _WEBSOCKETS_;
void WebSocketSession::registerKey(const std::string &key) {
    this->key = key;
    _WEBSOCKETS_.putSocket(key, this);
}
WebSocketSession::~WebSocketSession(){
    _WEBSOCKETS_.erase_and_close(key);
}
#endif