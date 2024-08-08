#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

class ChatServer
{
public:
    // 初始化TcpServer
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &nameArg)
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        // 通过绑定器设置回调函数
        _server.setConnectionCallback(bind(&ChatServer::onConnection,
                                           this, _1));

        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(bind(&ChatServer::onMessage,
                                        this, _1, _2, _3));

        // 设置EventLoop的线程个数 1 I/O    3 worker
        _server.setThreadNum(4);
    }

    // 启动ChatServer服务
    void start()
    {
        _server.start();
    }

private:
    // TcpServer绑定的回调函数，当有新连接或连接中断时调用
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << " State: OnLine!" << endl;
        }
        else
        {
            cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << " State: OffLine!" << endl;
            conn->shutdown();
            _loop->quit();
        }
    }

    // TcpServer绑定的回调函数，当有新数据时调用
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buffer,
                   Timestamp time)
    {
            string buf = buffer->retrieveAllAsString();
            cout << "Recv Data : " << buf << "Time: " << time.toString() << endl;
            conn->send(buf);
 
    }

    TcpServer _server;
    EventLoop *_loop;
};

int main()
{
    EventLoop loop; // epoll
    InetAddress addr("127.0.0.1", 9120);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    return 0;
}