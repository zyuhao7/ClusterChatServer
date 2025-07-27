#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
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
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关时间处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::AddGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接 redis服务器
    if (_redis.Connect())
    {
        LOG_INFO << "redis server connected success!";
        // 设置上报消息的回调函数
        _redis.Init_Notify_Handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}
// 服务异常, 重置用户状态
void ChatService::reset()
{
    // 下线状态设置
    _userModal.resetState();
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
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModal.query(id);
    if (user.GetId() == id && user.GetPwd() == pwd)
    {
        if (user.GetState() == "online")
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
            // 登陆成功.
            //  1. 更新用户状态信息
            user.SetState("online");
            _userModal.updateState(user);

            // 2. 记录用户连接信息
            {
                lock_guard<std::mutex> lock(_mtx);
                _userConnMap.insert({id, conn});
            }
            // 3. 用户登录成功后，向redis订阅 channel(id)
            _redis.Subscribe(id);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.GetId();
            response["name"] = user.GetName();

            // 4. 登录成功, 读取离线消息, 并发送
            vector<string> vec = _offlineMsgModal.query(id);
            if (vec.size() > 0)
            {
                response["offlinemsg"] = vec;
                // 读取后删除
                _offlineMsgModal.remove(id);
            }

            // 5. 查询好友的信息并返回
            vector<User> userVec = _friendModal.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.GetId();
                    js["name"] = user.GetName();
                    js["state"] = user.GetState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            // 6. 查询用户的群组信息
            vector<Group> groupuserVec = _groupModal.QueryUserGroupInfo(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.GetId();
                    grpjson["groupname"] = group.GetName();
                    grpjson["groupdesc"] = group.GetDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.GetUsers())
                    {
                        json js;
                        js["id"] = user.GetId();
                        js["name"] = user.GetName();
                        js["state"] = user.GetState();
                        js["role"] = user.GetRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
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
    if (state)
    {
        LOG_INFO << "注册成功";
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.GetId();
        conn->send(response.dump());
    }
    else
    {
        LOG_INFO << "注册失败";
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "reg failed";
        conn->send(response.dump());
    }
}
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<std::mutex> lock(_mtx);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.Unsubscribe(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModal.updateState(user);
}
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<std::mutex> lock(_mtx);

        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从 map表删除用户的连接信息.
                user.SetId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 更新用户状态信息
    if (user.GetId() != -1)
    {
        user.SetState("offline");
        _userModal.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<std::mutex> lock(_mtx);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // 转发消息
            it->second->send(js.dump());
            return;
        }
    }
    // 对方不在线, 存储离线消息
    _offlineMsgModal.insert(toid, js.dump());
}
// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModal.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储群组信息
    Group group(-1, name, desc);
    if (_groupModal.CreateGroup(group))
    {
        // 存储群组创建人信息
        _groupModal.AddGroup(userid, group.GetId(), "creator");
    }
    else
    {
        json response;
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "创建群组失败";
        conn->send(response.dump());
    }
}

void ChatService::AddGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModal.AddGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModal.QueryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_mtx);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发消息
            it->second->send(js.dump());
        }
        else
        {
            // 离线消息
            _offlineMsgModal.insert(id, js.dump());
        }
    }
}

void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_mtx);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModal.insert(userid, msg);
}