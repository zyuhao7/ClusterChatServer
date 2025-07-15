#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg)
    , _loop(loop)
{
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        std::cout << "ChatService::instance()->clientCloseException(conn);" << std::endl;
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();

    cout << buf << endl;
    // 数据反序列化
    json js = json::parse(buf);
    // 目的: 解耦网络模块和业务模块
    // 通过 js["msgid"] 获取业务模块对应的 handler 然后执行
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);
}