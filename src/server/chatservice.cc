#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <string>
using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

 void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "Do Login service!!";
    int id = js["id"];
    string pwd = js["password"];
    
    User user = _userModal.query(id);
    if(user.GetId() == id && user.GetPwd() == pwd)
    {
        std::cout<<user.GetState() <<std::endl;
        if(user.GetState() == "online")
        {
            // 该用户已经上线, 不允许重复登录.
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账户已经登陆..";
            conn->send(response.dump());
        }
        else
        {
            //登陆成功,
            // 1. 更新用户状态信息
            user.SetState("online");
            _userModal.updateState(user);
            
            // 2. 记录用户连接信息
            {
                lock_guard<std::mutex> lock(_mtx);
                _userConnMap.insert({id, conn});
            }

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.GetId();
            response["name"] = user.GetName();
            conn->send(response.dump());
        }
    }
    else
    {
        // 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或者密码错误";
        conn->send(response.dump());
    }
}

// name passward 
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.SetName(name);
    user.SetPwd(pwd);
    bool state = _userModal.Insert(user);
    if(state)
    {
        LOG_INFO<<"注册成功";
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.GetId();
        conn->send(response.dump());
    }
    else
    {
        LOG_INFO<<"注册失败";
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "reg failed";
        conn->send(response.dump());

    }
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        lock_guard<std::mutex> lock(_mtx);

        for(auto it = _userConnMap.begin();it != _userConnMap.end();it++)
        {
            if(it->second == conn)
            {
                // 从 map表删除用户的连接信息.
                user.SetId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    
    // 更新用户状态信息
    if(user.GetId() != -1)
    {
        user.SetState("offline");
        _userModal.updateState(user);
    }
}