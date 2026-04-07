#ifndef PUBLIC_H
#define PUBLIC_H

/*
server和client的公共文件
*/
enum EnMsgType
{
    LOGIN_MSG = 1,  // 登录消息
    LOGIN_MSG_ACK,  // 登录响应消息
    LOGINOUT_MSG,   // 注销消息
    LOGINOUT_MSG_ACK, // 注销响应消息
    REG_MSG,        // 注册消息
    REG_MSG_ACK,    // 注册响应消息
    ONE_CHAT_MSG,   // 聊天消息
    ONE_CHAT_MSG_ACK, // 聊天发送响应消息
    ADD_FRIEND_MSG, // 添加好友消息
    ADD_FRIEND_MSG_ACK, // 添加好友响应消息

    CREATE_GROUP_MSG,     // 创建群组
    CREATE_GROUP_MSG_ACK, // 创建群组响应消息
    ADD_GROUP_MSG,        // 加入群组
    ADD_GROUP_MSG_ACK,    // 加入群组响应消息
    GROUP_CHAT_MSG,       // 群聊天
    GROUP_CHAT_MSG_ACK,   // 群聊天发送响应消息
    LEAVE_GROUP_MSG,      // 退群
    LEAVE_GROUP_MSG_ACK,  // 退群响应
    SET_GROUP_ROLE_MSG,   // 调整群角色
    SET_GROUP_ROLE_MSG_ACK, // 调整群角色响应
    MARK_READ_MSG,        // 已读回执
    MARK_READ_MSG_ACK,    // 已读回执响应
    RECALL_MSG,           // 撤回消息
    RECALL_MSG_ACK,       // 撤回响应
    RECALL_NOTIFY_MSG,    // 撤回通知

};

#endif
