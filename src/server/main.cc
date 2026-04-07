#include "chatserver.hpp"
#include "chatservice.hpp"
#include "appconfig.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <cstdlib>
#include <signal.h>
#include <string>
using namespace std;

static muduo::Logger::LogLevel parseLogLevel(const std::string &log_level)
{
    if (log_level == "TRACE")
    {
        return muduo::Logger::TRACE;
    }
    if (log_level == "DEBUG")
    {
        return muduo::Logger::DEBUG;
    }
    if (log_level == "WARN")
    {
        return muduo::Logger::WARN;
    }
    if (log_level == "ERROR")
    {
        return muduo::Logger::ERROR;
    }
    if (log_level == "FATAL")
    {
        return muduo::Logger::FATAL;
    }
    return muduo::Logger::INFO;
}

// 处理 Ctrl + c 信号
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}
int main(int argc, char **argv)
{
    string config_path = "config/server.conf";
    string ip;
    uint16_t port = 0;

    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "--config")
        {
            if (i + 1 >= argc)
            {
                cerr << "missing value for --config" << endl;
                return -1;
            }
            config_path = argv[++i];
            continue;
        }

        if (ip.empty())
        {
            ip = arg;
            continue;
        }

        if (port == 0)
        {
            int p = atoi(arg.c_str());
            if (p <= 0 || p > 65535)
            {
                cerr << "invalid port: " << arg << endl;
                return -1;
            }
            port = static_cast<uint16_t>(p);
            continue;
        }
    }

    if (!AppConfig::instance().loadFromFile(config_path))
    {
        cerr << "failed to load config file: " << config_path << endl;
        cerr << "you can create one by copying config/server.conf.example" << endl;
        return -1;
    }

    if (ip.empty())
    {
        ip = AppConfig::instance().server().host;
    }
    if (port == 0)
    {
        port = AppConfig::instance().server().port;
    }

    muduo::Logger::setLogLevel(parseLogLevel(AppConfig::instance().server().log_level));

    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    /*
        
    */
}
