#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;
/// @file redis.hpp
/// @brief 定义 Redis 类，用于封装 Redis 的发布订阅功能

/// @class Redis
/// @brief Redis 类，封装了与 Redis 服务器的连接和发布订阅功能
///
/// 该类提供了与 Redis 服务器交互的功能，包括连接服务器、发布消息、订阅和取消订阅通道消息，
/// 以及在独立线程中监听订阅通道的消息。同时支持通过回调函数将订阅的消息上报到业务层。
///
/// 功能包括：
/// - 连接 Redis 服务器
/// - 发布消息到指定通道
/// - 订阅和取消订阅指定通道
/// - 在独立线程中监听订阅通道的消息
/// - 初始化消息上报的回调函数
///
/// @note 使用 hiredis 库实现 Redis 的同步通信
class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool Connect();

    // 向redis指定的通道channel发布消息
    bool Publish(int channel, string message);

    // 向redis指定的通道subscribe订阅消息
    bool Subscribe(int channel);

    // 向redis指定的通道unsubscribe取消订阅消息
    bool Unsubscribe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void Observer_Channel_Message();

    // 初始化向业务层上报通道消息的回调对象
    void Init_Notify_Handler(function<void(int, string)> fn);

private:
    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subcribe_context;

    // 回调操作，收到订阅的消息，给service层上报
    function<void(int, string)> _notify_message_handler;
};

#endif
