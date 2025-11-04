#pragma once

#include <unordered_set>
#include <functional>
#include <string>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <asyncreader.hpp>

class Command{
public:
using Arguments = std::vector<std::string>;
    Command(const std::string& name, std::function<void(Arguments args)> execution);
    Command(const Command& other) : name_(other.name_), execution_(std::move(other.execution_)){}
    const std::string& getName() const{
        return name_;
    }
    void execute(Arguments args) const{
        if(execution_) execution_(args);
    }
    bool operator==(const Command &other) const {
        return name_ == other.name_;
    }
private:
    std::string name_;
    std::function<void(Arguments args)> execution_;
};
namespace std {
    template <>
    struct hash<Command> {
        size_t operator()(const Command &c) const {
            return std::hash<std::string>()(c.getName());
        }
    };
}

class CommandExecutor {
using Arguments = std::vector<std::string>;
public:
    CommandExecutor(boost::asio::io_context& io) : io_(io), running_(true){
        auto reader = std::make_shared<AsyncCommandReader>(
            io_, [this](const std::string& cmd){
                _LOGGER_.log("Command typed: " + cmd);
                int space_1 = cmd.find(" ");
                std::string name = cmd.substr(0, space_1);
                Command::Arguments args = CommandExecutor::split(cmd.substr(space_1+1), ' ');
                this->runCommand(name, args);
            });

        std::thread cmd_thread([reader,this]{
            while(this->running_){
                _LOGGER_.commandLine(reader->get_cmd());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        cmd_thread.detach();

        std::thread input_thread([&io, reader]{
            auto work = boost::asio::make_work_guard(io);
            io.run();
            reader->stop();
        });
        input_thread.detach();
    }
    static std::string toLowerCase(const std::string& str){
        std::string copy = str;
        std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](unsigned char c){ return std::tolower(c); });
        return copy;
    }
    static std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }
    void runCommand(const std::string& cmd, Arguments args){
        std::string copy = CommandExecutor::toLowerCase(cmd);
        for(auto& c : set_) {
            if(copy == c.getName()) {
                c.execute(args);
                return;
            }
        }
        _LOGGER_.warning("Command " + cmd + " not found.");
    }
    void register_(const Command& cmd){
        set_.emplace(cmd);
    }
    void stop(){
        running_ = false;
        io_.stop();
    }
private:
    boost::asio::io_context& io_;
    std::unordered_set<Command> set_;
    std::atomic<bool> running_;
};

Command::Command(const std::string& name, std::function<void(Arguments args)> execution) : name_(CommandExecutor::toLowerCase(name)), execution_(std::move(execution)){}