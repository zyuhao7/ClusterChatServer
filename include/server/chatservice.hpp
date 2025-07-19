#ifndef CHATSERVICE_H
#define CHATSERVICE_H
#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "json.hpp"
#include "offlinemsgmodal.hpp"
#include "usermodal.hpp"
#include "friendmodal.hpp"
#include "groupmoodal.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService *instance();
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
      // 处理一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获得消息对应的处理器
    MsgHandler getHandler(int msg_id);
    // 客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    // 服务异常, 重置用户状态
    void reset();
    // 添加好友服务
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    // 创建群组服务
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time); 
    // 加入群组服务
    void AddGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    // 群聊天服务
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
private:
    ChatService();
    // 存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;
    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    
    // 数据操作类对象
    UserModal _userModal;
    OfflineMsgModal _offlineMsgModal;
    FriendModal _friendModal;
    GroupModal _groupModal;
    
    // 定义互斥锁, 保证 _userConnMap线程安全
    mutex _mtx;
};

#endif