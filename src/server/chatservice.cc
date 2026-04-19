#include "chatservice.hpp"
#include "public.hpp"
#include "appconfig.hpp"
#include "password.hpp"
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
    _msgHandlerMap.insert({LEAVE_GROUP_MSG, std::bind(&ChatService::leaveGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({SET_GROUP_ROLE_MSG, std::bind(&ChatService::setGroupRole, this, _1, _2, _3)});
    _msgHandlerMap.insert({MARK_READ_MSG, std::bind(&ChatService::markRead, this, _1, _2, _3)});
    _msgHandlerMap.insert({RECALL_MSG, std::bind(&ChatService::recallMessage, this, _1, _2, _3)});
    _msgHandlerMap.insert({QUERY_HISTORY_MSG, std::bind(&ChatService::queryHistory, this, _1, _2, _3)});
    _msgHandlerMap.insert({SEARCH_USER_MSG, std::bind(&ChatService::searchUser, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_BLACKLIST_MSG, std::bind(&ChatService::addBlacklist, this, _1, _2, _3)});
    _msgHandlerMap.insert({REMOVE_BLACKLIST_MSG, std::bind(&ChatService::removeBlacklist, this, _1, _2, _3)});
    _msgHandlerMap.insert({SET_USER_STATE_MSG, std::bind(&ChatService::setUserState, this, _1, _2, _3)});
    _msgHandlerMap.insert({SET_NICKNAME_MSG, std::bind(&ChatService::setNickname, this, _1, _2, _3)});

    // 连接 redis服务器
    if (_redis.Connect())
    {
        LOG_INFO << "redis server connected success!";
        // 设置上报消息的回调函数
        _redis.Init_Notify_Handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

string ChatService::requestIdFrom(const json &js) const
{
    if (!js.contains("request_id"))
    {
        return "";
    }
    return js["request_id"].get<string>();
}

void ChatService::sendAck(
    const TcpConnectionPtr &conn,
    int ack_msgid,
    const string &request_id,
    int err_no,
    const string &err_msg,
    const json &extra) const
{
    json response;
    response["version"] = CHAT_PROTOCOL_VERSION;
    response["msgid"] = ack_msgid;
    response["request_id"] = request_id;
    response["errno"] = err_no;
    response["errmsg"] = err_msg;

    for (auto it = extra.begin(); it != extra.end(); ++it)
    {
        response[it.key()] = it.value();
    }
    conn->send(response.dump());
}

void ChatService::unsubscribeUserChannel(int userid)
{
    if (userid <= 0)
    {
        return;
    }

    bool should_unsubscribe = false;
    {
        lock_guard<mutex> lock(_mtx);
        auto it = _subscribedUsers.find(userid);
        if (it != _subscribedUsers.end())
        {
            _subscribedUsers.erase(it);
            should_unsubscribe = true;
        }
    }

    if (should_unsubscribe)
    {
        _redis.Unsubscribe(userid);
    }
}

// 服务异常, 重置用户状态
void ChatService::reset()
{
    vector<int> subscribed_users;
    {
        lock_guard<mutex> lock(_mtx);
        _userConnMap.clear();
        subscribed_users.assign(_subscribedUsers.begin(), _subscribedUsers.end());
        _subscribedUsers.clear();
    }

    for (int userid : subscribed_users)
    {
        _redis.Unsubscribe(userid);
    }

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
    string pwd = js["password"].get<string>();
    string request_id = requestIdFrom(js);
    User user = _userModal.query(id);
    bool password_ok = user.GetId() == id && PasswordSecurity::verify(pwd, user.GetPwd());
    if (password_ok)
    {
        if (user.GetState() == "banned")
        {
            sendAck(conn, LOGIN_MSG_ACK, request_id, ERR_AUTH_BANNED, "用户已被封禁");
        }
        else if (user.GetState() == "online" || user.GetState() == "busy")
        {
            sendAck(conn, LOGIN_MSG_ACK, request_id, ERR_AUTH_ALREADY_ONLINE, "该账户已经登陆..");
        }
        else
        {
            if (!PasswordSecurity::isBcryptHash(user.GetPwd()))
            {
                string upgraded = PasswordSecurity::hashBcrypt(pwd, AppConfig::instance().server().bcrypt_cost);
                if (!upgraded.empty())
                {
                    _userModal.updatePassword(user.GetId(), upgraded);
                }
            }

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
            if (_redis.Subscribe(id))
            {
                lock_guard<std::mutex> lock(_mtx);
                _subscribedUsers.insert(id);
            }

            json extra;
            extra["id"] = user.GetId();
            extra["name"] = user.GetName();

            // 4. 登录成功, 读取离线消息, 并发送
            vector<string> vec = _offlineMsgModal.query(id);
            if (vec.size() > 0)
            {
                extra["offlinemsg"] = vec;
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
                extra["friends"] = vec2;
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
                extra["groups"] = groupV;
            }
            sendAck(conn, LOGIN_MSG_ACK, request_id, ERR_OK, "", extra);
        }
    }
    else
    {
        sendAck(conn, LOGIN_MSG_ACK, request_id, ERR_AUTH_INVALID_CREDENTIALS, "用户名或者密码错误");
    }
}

// name passward
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    string name = js["name"];
    string pwd = js["password"];
    if (name.empty() || pwd.empty())
    {
        sendAck(conn, REG_MSG_ACK, request_id, ERR_AUTH_EMPTY_CREDENTIALS, "用户名和密码不能为空");
        return;
    }

    string hashed_pwd = PasswordSecurity::hashBcrypt(pwd, AppConfig::instance().server().bcrypt_cost);
    if (hashed_pwd.empty())
    {
        sendAck(conn, REG_MSG_ACK, request_id, ERR_AUTH_HASH_FAILED, "密码哈希失败");
        return;
    }

    User user;
    user.SetName(name);
    user.SetPwd(hashed_pwd);
    bool state = _userModal.Insert(user);
    if (state)
    {
        LOG_INFO << "注册成功";
        json extra;
        extra["id"] = user.GetId();
        sendAck(conn, REG_MSG_ACK, request_id, ERR_OK, "", extra);
    }
    else
    {
        LOG_INFO << "注册失败";
        sendAck(conn, REG_MSG_ACK, request_id, ERR_AUTH_REGISTER_FAILED, "reg failed");
    }
}
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
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
    unsubscribeUserChannel(userid);

    // 更新用户状态信息
    User user(userid, "", "", "offline");
    _userModal.updateState(user);
    sendAck(conn, LOGINOUT_MSG_ACK, request_id, ERR_OK, "");
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
        unsubscribeUserChannel(user.GetId());
        user.SetState("offline");
        _userModal.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int toid = js["toid"].get<int>();

    if (_blacklistModal.isBlocked(userid, toid) || _blacklistModal.isBlocked(toid, userid))
    {
        sendAck(conn, ONE_CHAT_MSG_ACK, request_id, ERR_USER_BLOCKED_RELATION, "黑名单关系阻止发送消息");
        return;
    }

    long long existing_message_id = _messageHistoryModal.queryMessageIdByRequestId(request_id);
    if (existing_message_id > 0)
    {
        json extra;
        extra["message_id"] = existing_message_id;
        extra["duplicate"] = true;
        sendAck(conn, ONE_CHAT_MSG_ACK, request_id, ERR_OK, "", extra);
        return;
    }

    if (_userModal.query(toid).GetId() != toid)
    {
        sendAck(conn, ONE_CHAT_MSG_ACK, request_id, ERR_CHAT_TARGET_NOT_FOUND, "目标用户不存在");
        return;
    }

    long long message_id = _messageHistoryModal.insertDirect(request_id, userid, toid, js["msg"].get<string>());
    if (message_id < 0)
    {
        sendAck(conn, ONE_CHAT_MSG_ACK, request_id, ERR_CHAT_PERSIST_FAILED, "消息入库失败");
        return;
    }

    js["version"] = CHAT_PROTOCOL_VERSION;
    js["message_id"] = message_id;
    js["read_state"] = "unread";

    {
        lock_guard<std::mutex> lock(_mtx);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // 转发消息
            it->second->send(js.dump());
        }
        else
        {
            // 对方不在线, 存储离线消息
            _offlineMsgModal.insert(toid, js.dump(), request_id);
        }
    }

    json extra;
    extra["message_id"] = message_id;
    sendAck(conn, ONE_CHAT_MSG_ACK, request_id, ERR_OK, "", extra);
}
// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    if (userid == friendid)
    {
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_FRIEND_SELF, "不能添加自己为好友");
        return;
    }

    User friendUser = _userModal.query(friendid);
    if (friendUser.GetId() != friendid)
    {
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_FRIEND_TARGET_NOT_FOUND, "好友用户不存在");
        return;
    }

    if (_blacklistModal.isBlocked(userid, friendid) || _blacklistModal.isBlocked(friendid, userid))
    {
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_FRIEND_BLOCKED, "黑名单关系阻止添加好友");
        return;
    }

    if (_friendModal.isFriend(userid, friendid))
    {
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_FRIEND_ALREADY_FRIEND, "该用户已经是你的好友");
        return;
    }

    // 双向存储好友信息
    if (_friendModal.insert(userid, friendid))
    {
        json extra;
        extra["friendid"] = friendid;
        extra["friendname"] = friendUser.GetName();
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_OK, "", extra);
    }
    else
    {
        sendAck(conn, ADD_FRIEND_MSG_ACK, request_id, ERR_FRIEND_INSERT_FAILED, "添加好友失败");
    }
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    if (_userModal.query(userid).GetId() != userid)
    {
        sendAck(conn, CREATE_GROUP_MSG_ACK, request_id, ERR_GROUP_USER_NOT_FOUND, "用户不存在");
        return;
    }
    if (name.empty())
    {
        sendAck(conn, CREATE_GROUP_MSG_ACK, request_id, ERR_GROUP_NAME_EMPTY, "群组名称不能为空");
        return;
    }

    // 存储群组信息
    Group group(-1, name, desc);
    if (_groupModal.CreateGroup(group))
    {
        // 存储群组创建人信息
        if (_groupModal.AddGroup(userid, group.GetId(), "creator"))
        {
            json extra;
            extra["groupid"] = group.GetId();
            sendAck(conn, CREATE_GROUP_MSG_ACK, request_id, ERR_OK, "", extra);
            return;
        }
    }

    sendAck(conn, CREATE_GROUP_MSG_ACK, request_id, ERR_GROUP_CREATE_FAILED, "创建群组失败");
}

void ChatService::AddGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    if (!_groupModal.GroupExists(groupid))
    {
        sendAck(conn, ADD_GROUP_MSG_ACK, request_id, ERR_GROUP_NOT_FOUND, "群组不存在");
        return;
    }
    if (_userModal.query(userid).GetId() != userid)
    {
        sendAck(conn, ADD_GROUP_MSG_ACK, request_id, ERR_GROUP_USER_NOT_FOUND, "用户不存在");
        return;
    }
    if (_groupModal.IsUserInGroup(userid, groupid))
    {
        sendAck(conn, ADD_GROUP_MSG_ACK, request_id, ERR_GROUP_ALREADY_MEMBER, "用户已在群内");
        return;
    }
    if (_groupModal.AddGroup(userid, groupid, "normal"))
    {
        json extra;
        extra["groupid"] = groupid;
        sendAck(conn, ADD_GROUP_MSG_ACK, request_id, ERR_OK, "", extra);
    }
    else
    {
        sendAck(conn, ADD_GROUP_MSG_ACK, request_id, ERR_GROUP_JOIN_FAILED, "加入群组失败");
    }
}

void ChatService::leaveGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    if (!_groupModal.GroupExists(groupid))
    {
        sendAck(conn, LEAVE_GROUP_MSG_ACK, request_id, ERR_GROUP_NOT_FOUND, "群组不存在");
        return;
    }
    if (!_groupModal.IsUserInGroup(userid, groupid))
    {
        sendAck(conn, LEAVE_GROUP_MSG_ACK, request_id, ERR_GROUP_NOT_MEMBER, "用户不在群组中");
        return;
    }
    if (_groupModal.QueryUserRole(userid, groupid) == "creator")
    {
        sendAck(conn, LEAVE_GROUP_MSG_ACK, request_id, ERR_GROUP_CREATOR_CANNOT_LEAVE, "群主不能直接退群");
        return;
    }
    if (_groupModal.RemoveGroupUser(userid, groupid))
    {
        sendAck(conn, LEAVE_GROUP_MSG_ACK, request_id, ERR_OK, "");
    }
    else
    {
        sendAck(conn, LEAVE_GROUP_MSG_ACK, request_id, ERR_GROUP_LEAVE_FAILED, "退群失败");
    }
}

void ChatService::setGroupRole(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int operator_id = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    int target_id = js["targetid"].get<int>();
    string target_role = js["role"].get<string>();

    if (target_role != "normal" && target_role != "admin")
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_ROLE_INVALID, "角色非法");
        return;
    }
    if (!_groupModal.GroupExists(groupid))
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_NOT_FOUND, "群组不存在");
        return;
    }
    if (_groupModal.QueryUserRole(operator_id, groupid) != "creator")
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_ONLY_CREATOR_CAN_SET_ROLE, "仅群主可修改角色");
        return;
    }
    if (!_groupModal.IsUserInGroup(target_id, groupid))
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_TARGET_NOT_MEMBER, "目标用户不在群组中");
        return;
    }
    if (_groupModal.QueryUserRole(target_id, groupid) == "creator")
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_CANNOT_CHANGE_CREATOR_ROLE, "不能修改群主角色");
        return;
    }
    if (_groupModal.UpdateUserRole(target_id, groupid, target_role))
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_OK, "");
    }
    else
    {
        sendAck(conn, SET_GROUP_ROLE_MSG_ACK, request_id, ERR_GROUP_SET_ROLE_FAILED, "更新角色失败");
    }
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    long long existing_message_id = _messageHistoryModal.queryMessageIdByRequestId(request_id);
    if (existing_message_id > 0)
    {
        json extra;
        extra["message_id"] = existing_message_id;
        extra["deliver_count"] = 0;
        extra["duplicate"] = true;
        sendAck(conn, GROUP_CHAT_MSG_ACK, request_id, ERR_OK, "", extra);
        return;
    }

    if (!_groupModal.GroupExists(groupid))
    {
        sendAck(conn, GROUP_CHAT_MSG_ACK, request_id, ERR_GROUP_NOT_FOUND, "群组不存在");
        return;
    }
    if (!_groupModal.IsUserInGroup(userid, groupid))
    {
        sendAck(conn, GROUP_CHAT_MSG_ACK, request_id, ERR_GROUP_NOT_MEMBER, "用户不在群组中");
        return;
    }

    long long message_id = _messageHistoryModal.insertGroup(request_id, userid, groupid, js["msg"].get<string>());
    if (message_id < 0)
    {
        sendAck(conn, GROUP_CHAT_MSG_ACK, request_id, ERR_GROUP_CHAT_PERSIST_FAILED, "群聊消息入库失败");
        return;
    }

    js["version"] = CHAT_PROTOCOL_VERSION;
    js["message_id"] = message_id;
    js["read_state"] = "unread";
    vector<int> useridVec = _groupModal.QueryGroupUsers(userid, groupid);

    int deliver_count = 0;
    lock_guard<mutex> lock(_mtx);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发消息
            it->second->send(js.dump());
            ++deliver_count;
        }
        else
        {
            // 离线消息
            _offlineMsgModal.insert(id, js.dump(), request_id);
        }
    }

    json extra;
    extra["message_id"] = message_id;
    extra["deliver_count"] = deliver_count;
    sendAck(conn, GROUP_CHAT_MSG_ACK, request_id, ERR_OK, "", extra);
}

void ChatService::markRead(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    long long message_id = js["message_id"].get<long long>();

    if (_messageHistoryModal.markRead(message_id, userid))
    {
        sendAck(conn, MARK_READ_MSG_ACK, request_id, ERR_OK, "");
    }
    else
    {
        sendAck(conn, MARK_READ_MSG_ACK, request_id, ERR_MESSAGE_MARK_READ_FAILED, "标记已读失败");
    }
}

void ChatService::recallMessage(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    long long message_id = js["message_id"].get<long long>();

    if (!_messageHistoryModal.recallMessage(message_id, userid))
    {
        sendAck(conn, RECALL_MSG_ACK, request_id, ERR_MESSAGE_RECALL_FAILED, "撤回失败");
        return;
    }

    json notify;
    notify["version"] = CHAT_PROTOCOL_VERSION;
    notify["msgid"] = RECALL_NOTIFY_MSG;
    notify["request_id"] = request_id;
    notify["message_id"] = message_id;
    notify["operator_id"] = userid;

    if (js.contains("toid"))
    {
        int toid = js["toid"].get<int>();
        lock_guard<mutex> lock(_mtx);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            it->second->send(notify.dump());
        }
        else
        {
            _offlineMsgModal.insert(toid, notify.dump(), request_id);
        }
    }

    if (js.contains("groupid"))
    {
        int groupid = js["groupid"].get<int>();
        vector<int> users = _groupModal.QueryGroupUsers(userid, groupid);
        lock_guard<mutex> lock(_mtx);
        for (int uid : users)
        {
            auto it = _userConnMap.find(uid);
            if (it != _userConnMap.end())
            {
                it->second->send(notify.dump());
            }
            else
            {
                _offlineMsgModal.insert(uid, notify.dump(), request_id);
            }
        }
    }

    sendAck(conn, RECALL_MSG_ACK, request_id, ERR_OK, "");
}

void ChatService::queryHistory(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int limit = js.value("limit", 20);
    int offset = js.value("offset", 0);
    string order = js.value("order", "desc");
    bool ascending = (order == "asc");
    if (!ascending && order != "desc")
    {
        order = "desc";
    }
    if (limit <= 0)
    {
        limit = 20;
    }
    if (limit > 100)
    {
        limit = 100;
    }
    if (offset < 0)
    {
        offset = 0;
    }

    json extra;
    extra["limit"] = limit;
    extra["offset"] = offset;
    extra["order"] = order;

    if (js.contains("groupid"))
    {
        int groupid = js["groupid"].get<int>();
        if (!_groupModal.GroupExists(groupid))
        {
            sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_GROUP_NOT_FOUND, "群组不存在");
            return;
        }
        if (!_groupModal.IsUserInGroup(userid, groupid))
        {
            sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_GROUP_HISTORY_ACCESS_DENIED, "无权查看该群历史消息");
            return;
        }
        extra["history"] = _messageHistoryModal.queryGroupConversation(groupid, limit, offset, ascending);
        extra["scope"] = "group";
        extra["groupid"] = groupid;
        sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_OK, "", extra);
        return;
    }

    if (!js.contains("targetid"))
    {
        sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_MESSAGE_HISTORY_INVALID_SCOPE, "缺少历史消息查询目标");
        return;
    }

    int targetid = js["targetid"].get<int>();
    if (_userModal.query(targetid).GetId() != targetid)
    {
        sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_CHAT_TARGET_NOT_FOUND, "目标用户不存在");
        return;
    }

    extra["history"] = _messageHistoryModal.queryConversation(userid, targetid, limit, offset, ascending);
    extra["scope"] = "direct";
    extra["targetid"] = targetid;
    sendAck(conn, QUERY_HISTORY_MSG_ACK, request_id, ERR_OK, "", extra);
}

void ChatService::searchUser(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    string keyword = js.value("keyword", "");
    int limit = js.value("limit", 20);
    int offset = js.value("offset", 0);
    if (keyword.empty())
    {
        sendAck(conn, SEARCH_USER_MSG_ACK, request_id, ERR_USER_SEARCH_KEYWORD_EMPTY, "搜索关键字不能为空");
        return;
    }
    if (limit <= 0)
    {
        limit = 20;
    }
    if (limit > 50)
    {
        limit = 50;
    }
    if (offset < 0)
    {
        offset = 0;
    }

    vector<User> users = _userModal.searchByName(keyword, limit, offset);
    vector<string> payload;
    for (User &user : users)
    {
        if (user.GetId() == userid)
        {
            continue;
        }

        json item;
        item["id"] = user.GetId();
        item["name"] = user.GetName();
        item["state"] = user.GetState();
        item["is_friend"] = _friendModal.isFriend(userid, user.GetId());
        item["has_blocked"] = _blacklistModal.isBlocked(userid, user.GetId());
        item["blocked_by_target"] = _blacklistModal.isBlocked(user.GetId(), userid);
        payload.push_back(item.dump());
    }

    json extra;
    extra["keyword"] = keyword;
    extra["limit"] = limit;
    extra["offset"] = offset;
    extra["users"] = payload;
    sendAck(conn, SEARCH_USER_MSG_ACK, request_id, ERR_OK, "", extra);
}

void ChatService::addBlacklist(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int targetid = js["targetid"].get<int>();
    if (userid == targetid)
    {
        sendAck(conn, ADD_BLACKLIST_MSG_ACK, request_id, ERR_USER_BLOCK_SELF, "不能将自己加入黑名单");
        return;
    }
    if (_userModal.query(targetid).GetId() != targetid)
    {
        sendAck(conn, ADD_BLACKLIST_MSG_ACK, request_id, ERR_USER_BLOCK_TARGET_NOT_FOUND, "目标用户不存在");
        return;
    }
    if (_blacklistModal.isBlocked(userid, targetid))
    {
        sendAck(conn, ADD_BLACKLIST_MSG_ACK, request_id, ERR_USER_ALREADY_BLOCKED, "该用户已在黑名单中");
        return;
    }
    if (_blacklistModal.insert(userid, targetid))
    {
        json extra;
        extra["targetid"] = targetid;
        sendAck(conn, ADD_BLACKLIST_MSG_ACK, request_id, ERR_OK, "", extra);
    }
    else
    {
        sendAck(conn, ADD_BLACKLIST_MSG_ACK, request_id, ERR_USER_BLOCK_INSERT_FAILED, "加入黑名单失败");
    }
}

void ChatService::removeBlacklist(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    int targetid = js["targetid"].get<int>();
    if (!_blacklistModal.isBlocked(userid, targetid))
    {
        sendAck(conn, REMOVE_BLACKLIST_MSG_ACK, request_id, ERR_USER_NOT_BLOCKED, "目标用户不在黑名单中");
        return;
    }
    if (_blacklistModal.remove(userid, targetid))
    {
        json extra;
        extra["targetid"] = targetid;
        sendAck(conn, REMOVE_BLACKLIST_MSG_ACK, request_id, ERR_OK, "", extra);
    }
    else
    {
        sendAck(conn, REMOVE_BLACKLIST_MSG_ACK, request_id, ERR_USER_UNBLOCK_FAILED, "移出黑名单失败");
    }
}

void ChatService::setUserState(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    string state = js.value("state", "");
    if (state != "online" && state != "busy")
    {
        sendAck(conn, SET_USER_STATE_MSG_ACK, request_id, ERR_USER_STATE_INVALID, "状态非法，仅支持 online 或 busy");
        return;
    }

    bool has_connection = false;
    {
        lock_guard<mutex> lock(_mtx);
        auto it = _userConnMap.find(userid);
        has_connection = (it != _userConnMap.end() && it->second == conn);
    }
    if (!has_connection)
    {
        sendAck(conn, SET_USER_STATE_MSG_ACK, request_id, ERR_AUTH_INVALID_CREDENTIALS, "当前连接未登录");
        return;
    }

    User user = _userModal.query(userid);
    if (user.GetId() != userid)
    {
        sendAck(conn, SET_USER_STATE_MSG_ACK, request_id, ERR_GROUP_USER_NOT_FOUND, "用户不存在");
        return;
    }

    user.SetState(state);
    if (!_userModal.updateState(user))
    {
        sendAck(conn, SET_USER_STATE_MSG_ACK, request_id, ERR_USER_STATE_UPDATE_FAILED, "更新用户状态失败");
        return;
    }

    json extra;
    extra["state"] = state;
    sendAck(conn, SET_USER_STATE_MSG_ACK, request_id, ERR_OK, "", extra);
}

void ChatService::setNickname(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string request_id = requestIdFrom(js);
    int userid = js["id"].get<int>();
    string name = js.value("name", "");
    if (name.empty())
    {
        sendAck(conn, SET_NICKNAME_MSG_ACK, request_id, ERR_USER_NAME_EMPTY, "昵称不能为空");
        return;
    }

    bool has_connection = false;
    {
        lock_guard<mutex> lock(_mtx);
        auto it = _userConnMap.find(userid);
        has_connection = (it != _userConnMap.end() && it->second == conn);
    }
    if (!has_connection)
    {
        sendAck(conn, SET_NICKNAME_MSG_ACK, request_id, ERR_AUTH_INVALID_CREDENTIALS, "当前连接未登录");
        return;
    }

    User user = _userModal.query(userid);
    if (user.GetId() != userid)
    {
        sendAck(conn, SET_NICKNAME_MSG_ACK, request_id, ERR_GROUP_USER_NOT_FOUND, "用户不存在");
        return;
    }
    if (!_userModal.updateName(userid, name))
    {
        sendAck(conn, SET_NICKNAME_MSG_ACK, request_id, ERR_USER_NAME_UPDATE_FAILED, "更新昵称失败");
        return;
    }

    json extra;
    extra["name"] = name;
    sendAck(conn, SET_NICKNAME_MSG_ACK, request_id, ERR_OK, "", extra);
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
