#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/InetAddress.h>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

class ChatClient
{
public:
    ChatClient(EventLoop *loop, const InetAddress &serverAddr, const string &name)
        : _loop(loop), _client(loop, serverAddr, name)
    {
        _client.setConnectionCallback(bind(&ChatClient::onConnection, this, _1));
        _client.setMessageCallback(bind(&ChatClient::onMessage, this, _1, _2, _3));
        _client.enableRetry();
    }

    void connect()
    {
        _client.connect();
    }

    void disconnect()
    {
        _client.disconnect();
    }

    void send(const string &msg)
    {
        lock_guard<mutex> lock(_mutex);
        if (_connection)
        {
            _connection->send(msg);
        }
        else
        {
            cerr << "client is not connected yet" << endl;
        }
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        lock_guard<mutex> lock(_mutex);
        if (conn->connected())
        {
            cout << "Connected to " << conn->peerAddress().toIpPort() << endl;
            _connection = conn;
        }
        else
        {
            cout << "Disconnected from server" << endl;
            _connection.reset();
            _loop->quit();
        }
    }

    void onMessage(const TcpConnectionPtr &, Buffer *buffer, Timestamp time)
    {
        cout << "[" << time.toString() << "] recv: "
             << buffer->retrieveAllAsString() << endl;
    }

    EventLoop *_loop;
    TcpClient _client;
    TcpConnectionPtr _connection;
    mutex _mutex;
};

int main(int argc, char **argv)
{
    string ip = "127.0.0.1";
    uint16_t port = 9120;

    if (argc >= 3)
    {
        ip = argv[1];
        port = static_cast<uint16_t>(atoi(argv[2]));
    }

    EventLoop loop;
    InetAddress serverAddr(ip, port);
    ChatClient client(&loop, serverAddr, "MuduoChatClient");

    client.connect();

    thread inputThread([&client, &loop]()
                       {
                           cout << "Input message and press Enter. Type quit to exit." << endl;
                           string line;
                           while (getline(cin, line))
                           {
                               if (line == "quit")
                               {
                                   client.disconnect();
                                   loop.quit();
                                   break;
                               }
                               if (!line.empty())
                               {
                                   client.send(line);
                               }
                           } });

    loop.loop();

    if (inputThread.joinable())
    {
        inputThread.join();
    }

    return 0;
}
