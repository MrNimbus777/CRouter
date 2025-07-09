#include <iostream>

#include "server.hpp"
#include "config.hpp"
#include "default_request_handler.hpp"
#include "defaults.hpp"
#include "plugin_loader.hpp"

#ifdef _WIN32
#include <windows.h>
void enableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#else
void enableANSI() {}
#endif

Logger logger;

int main() {
    enableANSI();

    write_default_resources();

    loadConfig(CONF, "./.env");

    _PLUGINS_::loadPlugins("app/handlers");

    PluginInstance default_handler_instance = {std::unordered_map<std::string, void*>(), nullptr};
    std::function<Response(Request)> defaultHandler = nullptr;

    if (CONF.default_request_handler) {
        defaultHandler = _default_req_handler::func;
    } else {
        if (!std::filesystem::exists("./app/custom_default_request_handler.cpp")) {
            std::ofstream out("./app/custom_default_request_handler.cpp");
            out << custom_default_request_handler_cpp;
            out.close();
            return 0;
        }
        try {
            PluginInstance default_handler_instance = loadFunctions(compileFile("./app/custom_default_request_handler.cpp"), {"handle"});
            defaultHandler = (std::function<Response(Request)>)reinterpret_cast<Response (*)(Request)>(default_handler_instance.functions_map["handle"]);
        } catch (std::exception e) {
            logger.error("Failed to compile and load the custom_default_request_handler.cpp (" + std::string(e.what()) + ")");
            defaultHandler = _default_req_handler::func;
        }
    }

    try {
        boost::asio::io_context io_context;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(io_context.get_executor());

        boost::asio::thread_pool worker_pool(4);

        serv::Server server(io_context, 8080, worker_pool, [&defaultHandler](Request r) -> serv::Handler {
            serv::Handler h = {defaultHandler, false};

            std::string main_route = r.uri.size() > 1 ? r.uri.substr(1, r.uri.find("/", 1)) : "";

            IPlugin* pl = _PLUGINS_::getPlugin(main_route);
            if(pl){
                h.func = [&](Request request) -> Response { return pl->handle(request); };
                h.isHeavy = pl->isHeavy();
            }

            return h;
        });
        _LOGGER_.log("Server listening on port " + std::to_string(server.getPort()));

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
            if (!error) {
                _LOGGER_.log("Signal received (" + std::to_string(signal_number) + "), shutting down io_context...");
                io_context.stop();
                worker_pool.stop();
                worker_pool.join();
            } else {
                _LOGGER_.error("Signal wait error: " + error.message());
            }
        });

        std::vector<std::thread> io_threads;
        int io_thread_count = std::thread::hardware_concurrency();
        _LOGGER_.log("Starting " + std::to_string(io_thread_count) + " io_context threads...");
        for (int i = 0; i < io_thread_count; ++i) {
            io_threads.emplace_back([&]() {
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                std::string thread_id_str = oss.str();
                _LOGGER_.log("io_context thread " + thread_id_str + " starting run()...");
                io_context.run();
                _LOGGER_.log("io_context thread " + thread_id_str + " finished run().");
            });
        }

        _LOGGER_.log("Main thread participating in io_context.run()...");
        io_context.run();

        _LOGGER_.log("io_context has stopped. Waiting for all IO threads to finish...");
        for (std::thread& t : io_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        _LOGGER_.log("All threads joined. Server gracefully shut down.");

    } catch (std::exception& e) {
        _LOGGER_.log("Exception in main: " + std::string(e.what()));
    }
    freeLibrary(default_handler_instance);

    return 0;
}
