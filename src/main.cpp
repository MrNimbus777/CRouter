#include <iostream>
#include <chrono>

#include <server.hpp>
#include <config.hpp>
#include <default_request_handler.hpp>
#include <defaults.hpp>
#include <plugin_loader.hpp>
#include <command.hpp>
#include <lib_wrapper.hpp>


int main() {
    enableANSI();

    write_default_resources();

    loadConfig("./.env");

    Logger& logger = _LOGGER_;

    Config& config = CONF;

    _PLUGINS_::loadPlugins("./app/handlers");

    std::unique_ptr<LibWraper> def_lib = nullptr;
    std::shared_ptr<IPlugin> p_instance = nullptr;

    std::function<Response(Request&)> defaultHandler = nullptr;
    bool defaultHeavy = false;
    if (config.default_request_handler) {
        _default_req_handler::load();
        defaultHandler = _default_req_handler::func;
    } else {
        auto it = _PLUGINS_::loadedPlugins.find(config.custom_default_handler);
        if(it != _PLUGINS_::loadedPlugins.end()){
            def_lib = std::move(it->second.lib);
            p_instance = it->second.instance;
            defaultHandler = [&p_instance](Request& req) -> Response {
                return p_instance->handle(req);
            };
            defaultHeavy = p_instance->isHeavy();
            _PLUGINS_::loadedPlugins.erase(it);
            logger.log("Custom handler " + config.custom_default_handler + "loaded successfully.");
        } else {
            logger.error("Failed to find the custom handler " + config.custom_default_handler);
            _default_req_handler::load();
            defaultHandler = _default_req_handler::func;
        }
    }


    boost::asio::io_context io_commands;
    CommandExecutor exe(io_commands);
    
    try {
        boost::asio::io_context io_context;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(io_context.get_executor());

        Command exit_cmd("exit", [&io_context, &io_commands](Command::Arguments) {
            io_commands.stop();
            io_context.stop();
        });
        exe.register_(exit_cmd);
    

        boost::asio::thread_pool worker_pool(4);

        serv::Server server(io_context, config.port, worker_pool, [&defaultHandler, &defaultHeavy](Request& r) -> serv::Handler {
            serv::Handler h = {defaultHandler, defaultHeavy};

            std::string main_route = r.uri.size() > 1 ? r.uri.substr(1, r.uri.find("/", 1)-1) : "";
            
            auto pl = _PLUGINS_::getPlugin(main_route);
            if(pl){
                h.func = [&pl](Request& request) -> Response { return pl->handle(request); };
                h.isHeavy = pl->isHeavy();
            }

            return h;
        });
        exe.register_(Command("reload", [&](Command::Arguments args) {
            logger.log("Reloading ./.env . . .");
            loadConfig("./.env");
            logger.log("Reloaded ./.env");
            
            def_lib = nullptr;
            defaultHandler = nullptr;
            defaultHeavy = false;

            logger.log("Reloading ./app/handlers/ . . .");
            _PLUGINS_::clear();
            _PLUGINS_::loadPlugins("./app/handlers");
            logger.log("Reloaded ./app/handlers/");
            
            logger.log("Reloading default handler . . .");
            if (config.default_request_handler) {
                _default_req_handler::load();
                defaultHandler = _default_req_handler::func;
            } else {
                auto it = _PLUGINS_::loadedPlugins.find(config.custom_default_handler);
                if(it != _PLUGINS_::loadedPlugins.end()){
                    def_lib = std::move(it->second.lib);
                    p_instance = it->second.instance;
                    defaultHandler = [&p_instance](Request& req) -> Response {
                        return p_instance->handle(req);
                    };
                    defaultHeavy = p_instance->isHeavy();
                    _PLUGINS_::loadedPlugins.erase(it);
                    logger.log("Custom handler " + config.custom_default_handler + "loaded successfully.");
                } else {
                    logger.error("Failed to find the custom handler " + config.custom_default_handler);
                    _default_req_handler::load();
                    defaultHandler = _default_req_handler::func;
                }
            }
            logger.log("Reloaded default handler\n");

            logger.log("!!!");
            logger.log("!!! Some configurations need a full restart in order to apply! (e.g. port)");
            logger.log("!!!\n");

        }));

        logger.log("Server listening on port " + std::to_string(server.getPort()));

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
            if (!error) {
                logger.log("Signal received (" + std::to_string(signal_number) + "), shutting down io_context...");
                io_context.stop();
                worker_pool.stop();
                worker_pool.join();
            } else {
                logger.error("Signal wait error: " + error.message());
            }
        });

        std::vector<std::thread> io_threads;
        int io_thread_count = std::thread::hardware_concurrency();
        logger.log("Starting " + std::to_string(io_thread_count) + " io_context threads...");
        for (int i = 0; i < io_thread_count; i++) {
            io_threads.emplace_back([&]() {
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                std::string thread_id_str = oss.str();
                logger.log("io_context thread " + thread_id_str + " starting run()...");
                io_context.run();
                logger.log("io_context thread " + thread_id_str + " finished run().");
            });
        }

        logger.log("\033[32mMain thread participating in io_context.run()...");
        io_context.run();

        logger.log("io_context has stopped. Waiting for all IO threads to finish...");
        for (std::thread& t : io_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        logger.log("All threads joined. Server gracefully shut down.");

    } catch (std::exception& e) {
        logger.log("Exception in main: " + std::string(e.what()));
    }
    exe.stop();

    return 0;
}
