#pragma once

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "plugin.hpp"
#include "request.hpp"

namespace serv {

struct Handler {
    std::function<Response(Request)> func;
    bool isHeavy;
};
using HandlerBuilder = std::function<Handler(Request)>;

class Session : public std::enable_shared_from_this<Session> {
   public:
    Session(boost::asio::ip::tcp::socket socket, boost::asio::io_context& io_context, boost::asio::thread_pool& worker_pool, HandlerBuilder handler_builder)
        : socket_(std::move(socket)),
          io_context_(io_context),
          worker_pool_(worker_pool),
          handler_builder_(handler_builder),
          strand_(io_context) {}

    void start() {
        auto end_point = socket_.socket().remote_endpoint();
        _LOGGER_.log("New session started from: " + end_point.address().to_string() + ":" + std::to_string(end_point.port()));
        do_read_headers();
    }

   private:
    void do_close() {
        boost::system::error_code ec;
        socket_.socket().cancel(ec);
        socket_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.socket().close(ec);
        if (ec) {
            _LOGGER_.error("Error closing socket: " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
        }
        _LOGGER_.log("Session closed.");
    }
    void do_read_headers() {
        if (!socket_.socket().is_open()) {
            _LOGGER_.warning("Attempted to read from a closed socket. Aborting read operation.");
            return;
        }
        auto self = shared_from_this();
        socket_.expires_after(std::chrono::seconds(30));

        boost::asio::async_read_until(socket_.socket(), request_buffer_, "\r\n\r\n",
            boost::asio::bind_executor(strand_,
                [self](boost::system::error_code ec, std::size_t length) {
                    if (ec == boost::asio::error::operation_aborted) {
                        _LOGGER_.warning("Read headers timed out or was aborted for session.");
                        self->do_close();
                        return;
                    }
                    if (!ec) {
                        std::string full_request_content(
                            boost::asio::buffers_begin(self->request_buffer_.data()),
                            boost::asio::buffers_begin(self->request_buffer_.data()) + self->request_buffer_.size());
                        self->request_buffer_.consume(self->request_buffer_.size());

                        _LOGGER_.log("Request received:\n" + (full_request_content.length() > 512 ? (full_request_content.substr(0, (size_t)512) + "\n. . .") : full_request_content));

                        Request req = Request::parse(full_request_content);
                        Handler handler = self->handler_builder_(req);

                        auto func = handler.func;
                        if (handler.isHeavy) {
                            boost::asio::post(self->worker_pool_, [self, req, func]() {
                                try {
                                    Response res_obj = func(req);
                                    std::string response_str = res_obj.toString();

                                    boost::asio::post(self->strand_, [self, response_str]() {
                                        self->do_write(response_str);
                                    });
                                } catch (const std::exception& ex) {
                                    _LOGGER_.error("Error processing request: " + std::string(ex.what()));
                                    std::string error_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
                                    boost::asio::post(self->strand_, [self, error_response]() {
                                        boost::asio::async_write(self->socket_, boost::asio::buffer(error_response),
                                            [self](boost::system::error_code write_ec, std::size_t) {
                                                if (write_ec) {
                                                    _LOGGER_.error("Error sending response: " + write_ec.message() + " (Error code: " + std::to_string(write_ec.value()) + ")");
                                                }
                                                self->do_close();
                                            });
                                    });
                                }
                            });
                        } else {
                            try {
                                Response res_obj = func(req);
                                std::string response_str = res_obj.toString();

                                boost::asio::post(self->strand_, [self, response_str]() {
                                    self->do_write(response_str);
                                });
                            } catch (const std::exception& ex) {
                                _LOGGER_.error("Error processing request: " + std::string(ex.what()));
                                std::string error_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
                                boost::asio::post([self, error_response]() {
                                    boost::asio::async_write(self->socket_, boost::asio::buffer(error_response),
                                        [self](boost::system::error_code write_ec, std::size_t) {
                                            if (write_ec) {
                                                _LOGGER_.error("Error sending response: " + write_ec.message() + " (Error code: " + std::to_string(write_ec.value()) + ")");
                                            }
                                            self->do_close();
                                        });
                                });
                            }
                        }
                    } else {
                        if (ec == boost::asio::error::timed_out) {
                            _LOGGER_.log("Read operation timed out or was aborted for session.");
                            self->do_close();
                            return;
                        }
                        _LOGGER_.error("Error during read headers: " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
                        self->do_close();
                    }
                }));
    }

    void do_write(const std::string& response) {
        if (!socket_.socket().is_open()) {
            _LOGGER_.warning("Attempted to write to a closed socket. Aborting write operation.");
            return;
        }
        auto self = shared_from_this();
        socket_.expires_after(std::chrono::seconds(30));
        auto response_ptr = std::make_shared<std::string>(std::move(response));

        boost::asio::async_write(socket_.socket(), boost::asio::buffer(*response_ptr),
            boost::asio::bind_executor(strand_,
                [self, response_ptr](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (ec == boost::asio::error::operation_aborted) {
                        _LOGGER_.log("Write operation timed out or was aborted for session (Error code: " + std::to_string(ec.value()) + ")");
                        self->do_close();
                        return;
                    }
                    if (!ec) {
                        self->do_read_headers();
                    } else {
                        _LOGGER_.error("Error during write: " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
                        self->do_close();
                    }
                }));
    }

    boost::beast::tcp_stream socket_;
    boost::asio::io_context& io_context_;
    boost::asio::thread_pool& worker_pool_;
    HandlerBuilder handler_builder_;
    boost::asio::io_context::strand strand_;
    boost::asio::streambuf request_buffer_;
};

class Server {
   public:
    Server(boost::asio::io_context& io_context, unsigned short port, boost::asio::thread_pool& worker_pool, HandlerBuilder handler_builder)
        : io_context_(io_context),
          acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
          worker_pool_(worker_pool),
          handler_builder_(handler_builder),
          port_(port) {
        do_accept();
    }

    unsigned short getPort() const {
        return port_;
    }

   private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), io_context_, worker_pool_, handler_builder_)->start();
                } else {
                    _LOGGER_.error("Accept error: " + ec.message());
                }
                do_accept();
            });
    }

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::thread_pool& worker_pool_;
    HandlerBuilder handler_builder_;
    unsigned short port_;
};

}  // namespace serv