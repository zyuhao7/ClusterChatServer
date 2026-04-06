#ifndef FRIENDMODAL_H
#define FRIENDMODAL_H
#include "user.hpp"
#include <vector>
using namespace std;
// 维护好友信息模块
class FriendModal
{
public:
    // 添加好友关系
    bool insert(int userid, int friendid);
    // 判断是否已经是好友
    bool isFriend(int userid, int friendid);
    // 返回好友列表
    vector<User> query(int userid);
};

#endif
