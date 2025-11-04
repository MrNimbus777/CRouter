#ifndef PLUGIN_HPP
#define PLUGIN_HPP

#ifdef _WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <deque>
#include <iostream>
#include <functional>
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
    virtual bool isHeavy(){return false;};
    virtual void onLoad(){};
    IPlugin *setLogger(ILogger *logger) {
        this->_LOGGER_ = logger;
        return this;
    }
    IPlugin *setWSM(IWebSocketPool *websockets) {
        this->_WEBSOCKETS_ = websockets;
        return this;
    }
    virtual ~IPlugin() = default;

   protected:
    ILogger *_LOGGER_ = nullptr;
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

#ifdef _WIN32
#include <windows.h>
void hideCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}
void showCursor(){
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}
void enableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#else
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
void hideCursor() {
    std::cout << "\e[?25l" << std::flush;
}
void showCursor(){
    std::cout << "\e[?25h" << std::flush;
}
void enableANSI() {}
#endif

class Logger : public ILogger {
   public:
    Logger(){
        std::cout << "\n\n";
    }
    void commandLine(const std::string& cmd){
        if(!to_print) return;
        std::lock_guard<std::mutex> lock(mutex_);
        hideCursor();
        std::cout << clrl << "\r> " << cmd;
        showCursor();
        to_print = false;
    }
    void log(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\r" << clrl << upl << clrl << upl << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n\n\n";
        to_print = true;
    }
    void warning(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\r" << clrl << upl << clrl << upl << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] \033[33m[WhuiRNING] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n\n\n";
        showCursor();
        to_print = true;
    }
    void error(const std::string &message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\r" << clrl << upl << clrl << upl << "\033[0m[" << __PLUGIN_HELPER__::getTime() << "][T" << std::this_thread::get_id() << "] \033[31m[ERROR] " << __PLUGIN_HELPER__::replace_all(message, "\n", "\n    ") << "\033[0m\n\n\n";
        to_print = true;
    }
    void update(){
        to_print = true;
    }

   private:
    bool to_print = true;
    std::mutex mutex_;
    inline static const char* upl = "\033[A";
    inline static const char* clrl = "\033[2K";
};

Logger _LOGGER_;

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