#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"
#include "appconfig.hpp"
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
    _server.setThreadNum(AppConfig::instance().server().thread_num);
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

    try
    {
        json js = json::parse(buf);
        if (!js.contains("msgid"))
        {
            json resp;
            resp["msgid"] = -1;
            resp["request_id"] = js.value("request_id", "");
            resp["errno"] = 400;
            resp["errmsg"] = "missing msgid";
            conn->send(resp.dump());
            return;
        }

        auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
        msgHandler(conn, js, time);
    }
    catch (const std::exception &e)
    {
        json resp;
        resp["msgid"] = -1;
        resp["request_id"] = "";
        resp["errno"] = 400;
        resp["errmsg"] = string("invalid json: ") + e.what();
        conn->send(resp.dump());
    }
}
