#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <atomic>

#if defined(_WIN32)
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

class AsyncCommandReader {
public:
    using CommandHandler = std::function<void(const std::string&)>;

    AsyncCommandReader(boost::asio::io_context& io, CommandHandler handler)
        : io_(io), handler_(std::move(handler)), running_(true), cursor_pos_(0)
    {
#if defined(_WIN32)
        readerThread_ = std::thread([this]() { readLoopWin(); });
#else
        tcgetattr(STDIN_FILENO, &oldt_);
        termios newt = oldt_;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        input_ = std::make_unique<boost::asio::posix::stream_descriptor>(io_, ::dup(STDIN_FILENO));
        startReadPosix();
#endif
    }

    ~AsyncCommandReader() {
        stop();
    }
    std::string& get_cmd(){
        return current_cmd_;
    } 
    void stop() {
        if (!running_) return;
        running_ = false;
#if defined(_WIN32)
        if (readerThread_.joinable()) readerThread_.join();
#else
        if (input_ && input_->is_open()) input_->close();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt_);
#endif
    }

private:
    void processChar(int c) {
#if defined(_WIN32)
        static bool arrow_prefix = false;
        if (c == 0 || c == 224) {
            arrow_prefix = true;
            return;
        }
        if (arrow_prefix) {
            if (c == 75) {
                if (cursor_pos_ > 0) {
                    cursor_pos_--;
                    std::cout << "\b" << std::flush;
                }
            } else if (c == 77) {
                if (cursor_pos_ < current_cmd_.size()) {
                    std::cout << current_cmd_[cursor_pos_];
                    cursor_pos_++;
                }
            }
            arrow_prefix = false;
            return;
        }
#endif

        switch (c) {
            case 127:
            case 8:
                if (cursor_pos_ > 0) {
                    current_cmd_.erase(cursor_pos_ - 1, 1);
                    cursor_pos_--;
                    _LOGGER_.update();
                }
                break;

            case '\r':
            case '\n':
                std::cout << std::endl;
                if (handler_) handler_(current_cmd_);
                current_cmd_.clear();
                cursor_pos_ = 0;
                _LOGGER_.update();
                break;

            default:
                current_cmd_.insert(cursor_pos_, 1, ((char) c));
                cursor_pos_++;
                _LOGGER_.update();
                break;
        }
    }

#if defined(_WIN32)
    void readLoopWin() {
        while (running_) {
            if (_kbhit()) {
                int c = _getch();
                boost::asio::post(io_, [this, c]() { processChar(c); });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
#else
    void startReadPosix() {
        auto buf = std::make_shared<char>();
        input_->async_read_some(boost::asio::buffer(buf.get(), 1),
            [this, buf](const boost::system::error_code& ec, std::size_t n) {
                if (!ec && n > 0 && running_) {
                    char c = *buf;
                    processChar(c);
                    startReadPosix();
                }
            });
    }
#endif

    boost::asio::io_context& io_;
    CommandHandler handler_;
    std::atomic<bool> running_;

    std::string current_cmd_;
    size_t cursor_pos_;

#if defined(_WIN32)
    std::thread readerThread_;
#else
    std::unique_ptr<boost::asio::posix::stream_descriptor> input_;
    termios oldt_;
#endif
};
