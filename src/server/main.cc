#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

using namespace std;

// 处理 Ctrl + c 信号
void resetHanlder(int)
{
    ChatService::instance()->reset();
    exit(0);
}
int main()
{
    signal(SIGINT, resetHanlder);
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    // {"msgid":1}
    // {"msgid":4,"name":"myh","password":"123456"}
    // {"msgid":4,"name":"xh","password":"123456"} 注册
    // {"msgid": 1, "id":2, "password":"123456"}  登录
    // {"msgid":6,"id":1,"from":"myh","to":2,"msg":"hello"} 一对一聊天
    // {"msgid":7, "id":2, "friendid":1} 添加好友
}