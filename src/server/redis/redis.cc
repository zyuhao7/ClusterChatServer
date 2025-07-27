#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis()
    : _publish_context(nullptr),
      _subcribe_context(nullptr)
{
}
Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }

    if (_subcribe_context != nullptr)
    {
        redisFree(_subcribe_context);
    }
}

bool Redis::Connect()
{
    // 负责 publish 发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _publish_context)
    {
        cerr << "connect redis server failed!" << endl;
        return false;
    }

    // 负责 subscribe 订阅消息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subcribe_context)
    {
        cerr << "connect redis server failed!" << endl;
        return false;
    }
    // 在单独的线程中,监听通道上的事件, 有消息就给业务层上报
    thread t([&]()
             { Observer_Channel_Message(); });
    t.detach();
    cout << "Connect redis-server success!" << endl;
    return true;
}

bool Redis::Publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (nullptr == reply)
    {
        cerr << "publish message failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool Redis::Subscribe(int channal)
{
    /*
        SUBSCRIBE 命令本身会造成线程阻塞等待通道里面发生的消息, 这里只做订阅通道, 不接收消息
        通道消息的接收专门在 Observer_Channel_Message 函数中进行
    */
    if (REDIS_ERR == redisAppendCommand(_subcribe_context, "SUBSCRIBE %d", channal))
    {
        cerr << "subscribe channel failed!" << endl;
        return false;
    }
    // redisBufferWrite 函数会一直阻塞, 直到缓冲区写满或者写完
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subcribe_context, &done))
        {
            cerr << "subscribe channel failed!" << endl;
            return false;
        }
    }
    return true;
}

bool Redis::Unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(_subcribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe channel failed!" << endl;
        return false;
    }
    // redisBufferWrite 函数会一直阻塞, 直到缓冲区发送完毕
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(_subcribe_context, &done))
        {
            cerr << "unsubscribe channel failed!" << endl;
            return false;
        }
    }
    return true;
}

void Redis::Observer_Channel_Message()
{
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(_subcribe_context, (void **)&reply))
    {
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道发生的消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }
    cerr << ">>>>>>>>>>>>>>>>>> Observer_Channel_Message thread exit! <<<<<<<<<<<<<<<<<<<" << endl;
}

void Redis::Init_Notify_Handler(function<void(int, string)> fn)
{
    _notify_message_handler = fn;
}